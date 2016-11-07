#include <cil/android.h>
#include <sepol/policydb/hashtab.h>
#include <stdlib.h>
#include <string.h>

#include "cil_build_ast.h"
#include "cil_internal.h"
#include "cil_strpool.h"
#include "cil_symtab.h"
#include "cil_tree.h"

#define VER_MAP_SZ (1 << 12)

/* added to hashmap - currently unused as hashmap is used as a set */
struct version_datum {
	struct cil_db *db;
	struct cil_tree_node *ast_node;
	char *orig_name;
};

struct version_args {
	struct cil_db *db;
	hashtab_t vers_map;
	const char *num;
};

static unsigned int ver_map_hash_val(hashtab_t h, const hashtab_key_t key) {
	/* from cil_stpool.c */
	char *p, *keyp;
	size_t size;
	unsigned int val;

	val = 0;
	keyp = (char*)key;
	size = strlen(keyp);
	for (p = keyp; ((size_t) (p - keyp)) < size; p++)
		val =
			(val << 4 | (val >> (8 * sizeof(unsigned int) - 4))) ^ (*p);
	return val & (h->size - 1);
}


static int ver_map_key_cmp(hashtab_t h __attribute__ ((unused)),
		    const hashtab_key_t key1, const hashtab_key_t key2) {
	/* hashtab_key_t is just a char* underneath */
	return strcmp((char *)key1, (char *)key2);
}

/*
 * version_datum  pointers all refer to memory owned elsewhere, so just free the
 * datum itself.
 */
static int ver_map_entry_destroy(__attribute__ ((unused))hashtab_key_t k,
				 hashtab_datum_t d, __attribute__ ((unused))void *args) {
	free(d);
	return 0;
}

static void ver_map_destroy(hashtab_t h) {
	hashtab_map(h, ver_map_entry_destroy, NULL);
	hashtab_destroy(h);
}

static int __extract_attributees_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args) {
	int rc = SEPOL_ERR;
	struct version_args *args = (struct version_args *) extra_args;
	char *key;
	struct version_datum *datum;

	if (node == NULL || finished == NULL || extra_args == NULL) {
		goto exit;
	}

	switch (node->flavor) {
	case CIL_ROLE:
		cil_log(CIL_ERR, "%s should not be used in platform public policy (line %d)\n",
			CIL_KEY_ROLE, node->line);
		rc = SEPOL_ERR;
		break;
	case CIL_TYPE:
	case CIL_TYPEATTRIBUTE:
		datum = cil_malloc(sizeof(*datum));
		datum->db = args->db;
		datum->ast_node = node;
		datum->orig_name = DATUM(node->data)->name;
		key = datum->orig_name;
		if (!strncmp(key, "base_typeattr_", 14)) {
			/* checkpolicy creates base attributes which are just typeattributesets,
			   of the existing types and attributes.  These may be differnt in
			   every checkpolicy output, ignore them here, they'll be dealt with
			   as a special case when attributizing. TODO: remove these? */
		} else {
			rc = hashtab_insert(args->vers_map, (hashtab_key_t) key,
					    (hashtab_datum_t) datum);
			if (rc != SEPOL_OK) {
				goto exit;
			}
		}
		break;
	case CIL_TYPEALIAS:
		cil_log(CIL_ERR, "%s should not be used in platform public policy (line %d)\n",
			CIL_KEY_TYPEALIAS, node->line);
		goto exit;
		break;
	case CIL_TYPEPERMISSIVE:
		cil_log(CIL_ERR, "%s should not be used in platform public policy (line %d)\n",
			CIL_KEY_TYPEPERMISSIVE, node->line);
		goto exit;
		break;
	case CIL_NAMETYPETRANSITION:
	case CIL_TYPE_RULE:
		cil_log(CIL_ERR, "%s should not be used in platform public policy (line %d)\n",
			CIL_KEY_TYPETRANSITION, node->line);
		goto exit;
		break;
	default:
		break;
	}
	return SEPOL_OK;
exit:
	return rc;
}

/*
 * For the given db, with an already-built AST, fill the vers_map hash table
 * with every encountered type and attribute.  This could eventually be expanded
 * to include other language constructs, such as users and roles, in which case
 * multiple hash tables would be needed.  These tables can then be used by
 * attributize() to change all references to these types.
 */
int cil_extract_attributees(struct cil_db *db, hashtab_t vers_map) {
	/* walk ast. */
	int rc = SEPOL_ERR;
	struct version_args extra_args;
	extra_args.db = db;
	extra_args.vers_map = vers_map;
	extra_args.num = NULL;
	rc = cil_tree_walk(db->ast->root, __extract_attributees_helper, NULL, NULL, &extra_args);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;
exit:
	return rc;
}

/*
 * Takes the old name and version string and creates a new strpool entry by
 * combining them.
 */
static char *__cil_attrib_get_versname(char *old, const char *vers) {
	size_t len = 0;
	char *tmp_new = NULL;
	char *final;

	len += strlen(old) + strlen(vers) + 2;
	tmp_new = cil_malloc(len);
	sprintf(tmp_new, "%s_%s", old, vers);
	final = cil_strpool_add(tmp_new);
	free(tmp_new);
	return final;
}

/*
 * Change type to attribute - create new versioned name based on old, create
 * typeattribute node and replace existing type node.
 */
static int __cil_attrib_convert_type(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_type *type = (struct cil_type *)node->data;
	struct cil_typeattribute *typeattr = NULL;
	char *new_key;

	cil_typeattribute_init(&typeattr);

	new_key = __cil_attrib_get_versname(type->datum.name, args->num);

	cil_symtab_datum_remove_node(&type->datum, node);
	cil_destroy_type(type);

	rc = cil_gen_node(args->db, node, (struct cil_symtab_datum *) typeattr,
			  new_key, CIL_SYM_TYPES, CIL_TYPEATTRIBUTE);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;
exit:
	return rc;
}

/*
 * Update datum - create new key, remove entry under old key,
 * update entry, and insert under new key
 */
static int __cil_attrib_swap_symtab_key(struct cil_tree_node *node, char *old_key,
				 const char *num) {
	int rc = SEPOL_ERR;
	char *new_key;
	symtab_t *symtab;
	struct cil_symtab_datum *datum = (struct cil_symtab_datum *) node->data;

	new_key = __cil_attrib_get_versname(old_key, num);

	symtab = datum->symtab;

	/* TODO: remove, but what happens to other nodes on this datum ?*/
	cil_list_remove(datum->nodes, CIL_NODE, node, 0);
	cil_symtab_remove_datum(datum);

	rc = cil_symtab_insert(symtab, new_key, datum, node);

	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;
exit:
	return rc;
}

/* TODO - move to shared header? Copied from cil_build_ast.c */
static enum cil_flavor __cil_get_expr_operator_flavor(const char *op)
{
	if (op == NULL) return CIL_NONE;
	else if (op == CIL_KEY_AND)   return CIL_AND;
	else if (op == CIL_KEY_OR)    return CIL_OR;
	else if (op == CIL_KEY_NOT)   return CIL_NOT;
	else if (op == CIL_KEY_EQ)    return CIL_EQ;    /* Only conditional */
	else if (op == CIL_KEY_NEQ)   return CIL_NEQ;   /* Only conditional */
	else if (op == CIL_KEY_XOR)   return CIL_XOR;
	else if (op == CIL_KEY_ALL)   return CIL_ALL;   /* Only set and permissionx */
	else if (op == CIL_KEY_RANGE) return CIL_RANGE; /* Only catset and permissionx */
	else return CIL_NONE;
}

/*
 * expressions may contains strings which are not in the type-attribute
 * namespace, so this is not a general cil_expr attributizer.
 * TODO: add support for other types of expressions which may contain types.
 */
static int cil_attrib_type_expr(struct cil_list *expr_str, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_list_item *curr = NULL;
	char *new;

	/* iterate through cil_list, replacing types */
	cil_list_for_each(curr, expr_str) {
		switch(curr->flavor) {
		case CIL_LIST:
			rc = cil_attrib_type_expr((struct cil_list *)curr->data, args);
			if (rc != SEPOL_OK)
				goto exit;
			break;
		case CIL_STRING:
			if (hashtab_search(args->vers_map, (hashtab_key_t) curr->data) != NULL) {
				new = __cil_attrib_get_versname((char *) curr->data, args->num);
				curr->data = (void *) new;
			} else if (__cil_get_expr_operator_flavor((char *)curr->data) == CIL_NONE) {
				new = __cil_attrib_get_versname((char *) curr->data, NON_PLAT_SUFFIX);
				curr->data = (void *) new;
			}
			break;
		case CIL_DATUM:
			cil_log(CIL_ERR, "AST already resolved. Not yet supported.\n");
			rc = SEPOL_ERR;
			goto exit;
			break;
		default:
			break;
		}
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_check_context(struct cil_context *ctxt, struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;

	if (ctxt->type != NULL) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported.\n");
		goto exit;
	}
	key = ctxt->type_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) == NULL) {
		ctxt->type_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_sidcontext(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_sidcontext *sidcon = (struct cil_sidcontext *)node->data;

	if (sidcon->context_str != NULL) {
		/* sidcon refers to a named context, which will be modified elsewhere */
	} else {
		/* sidcon contains an anon context, which needs to have type checked */
		rc = cil_attrib_check_context(sidcon->context, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_context(struct cil_tree_node *node, struct version_args *args) {
	struct cil_context *ctxt = (struct cil_context *)node->data;

	return cil_attrib_check_context(ctxt, args);
}

static int cil_attrib_roletype(struct cil_tree_node *node,
                        __attribute__((unused)) struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;
	struct cil_roletype *roletype = (struct cil_roletype *)node->data;

	if (roletype->role) {
		cil_log(CIL_ERR, "AST already resolved.  !!! Not yet supported.\n");
		goto exit;
	}
	key = roletype->type_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		roletype->type_str = __cil_attrib_get_versname(key, args->num);
	} else {
		roletype->type_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_type(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_type *type = (struct cil_type *)node->data;
	char *key = type->datum.name;

	if (type->value) {
		cil_log(CIL_ERR, "AST already resolved.  !!! Not yet supported.\n");
		goto exit;
	}
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		rc = __cil_attrib_convert_type(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	} else {
		/* non-platform type: give it _np suffix to avoid name conflicts.
		   TODO: investigate namespaces (cil only) instead */
		rc = __cil_attrib_swap_symtab_key(node, key, NON_PLAT_SUFFIX);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_typepermissive(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_typepermissive *typeperm = (struct cil_typepermissive *)node->data;
	char *key;

	if (typeperm->type != NULL) {
		cil_log(CIL_ERR, "AST already resolved.  ### Not yet supported.\n");
		goto exit;
	}
	key = typeperm->type_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		cil_log(CIL_ERR, "%s contains platform public type: %s (line %d) .\n",
			CIL_KEY_TYPEPERMISSIVE, key, node->line);
		goto exit;
	} else {
		typeperm->type_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_typeattribute(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_typeattribute *typeattr = (struct cil_typeattribute *)node->data;
	char *key = typeattr->datum.name;

	if (typeattr->types) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported (line %d).\n",
			node->line);
		goto exit;
	}
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		rc = __cil_attrib_swap_symtab_key(node, key, args->num);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	} else {
		rc = __cil_attrib_swap_symtab_key(node, key, NON_PLAT_SUFFIX);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_typeattributeset(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;
	struct cil_typeattributeset *typeattrset = (struct cil_typeattributeset *) node->data;

	if (typeattrset->datum_expr != NULL) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported (line %d).\n",
			node->line);
		goto exit;
	}

	key = typeattrset->attr_str;
	/* first check to see if the attribute to which this set belongs is versioned */
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		typeattrset->attr_str = __cil_attrib_get_versname(key, args->num);
	} else {
		typeattrset->attr_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}
	rc = cil_attrib_type_expr(typeattrset->str_expr, args);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_typealiasactual(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;
	struct cil_aliasactual *aliasact = (struct cil_aliasactual *)node->data;

	key = aliasact->actual_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		cil_log(CIL_ERR, "%s with platform public type not allowed (line %d)\n",
		    CIL_KEY_TYPEALIASACTUAL, node->line);
		goto exit;
	} else {
		aliasact->actual_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_nametypetransition(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;
	struct cil_nametypetransition *namettrans = (struct cil_nametypetransition *)node->data;

	if (namettrans->src != NULL) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported (line %d).\n",
			node->line);
		goto exit;
	}
	key = namettrans->src_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		namettrans->src_str = __cil_attrib_get_versname(key, args->num);
	} else {
		namettrans->src_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}
	key = namettrans->tgt_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		namettrans->tgt_str = __cil_attrib_get_versname(key, args->num);
	} else {
		namettrans->tgt_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}
	key = namettrans->result_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) == NULL) {
		namettrans->result_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}

	return SEPOL_OK;
exit:
	return rc;
}

/*
 * This is exactly the same as cil_attrib_nametypetransition, but the struct
 * layouts differ, so we can't reuse it.
 */
static int cil_attrib_type_rule(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;
	struct cil_type_rule *type_rule = (struct cil_type_rule *)node->data;

	if (type_rule->src != NULL) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported (line %d).\n",
			node->line);
		goto exit;
	}
	key = type_rule->src_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		type_rule->src_str = __cil_attrib_get_versname(key, args->num);
	} else {
		type_rule->src_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}
	key = type_rule->tgt_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		type_rule->tgt_str = __cil_attrib_get_versname(key, args->num);
	} else {
		type_rule->tgt_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}
	key = type_rule->result_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) == NULL) {
		type_rule->result_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_avrule(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;
	struct cil_avrule *avrule = (struct cil_avrule *)node->data;

	if (avrule->src != NULL) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported (line %d).\n",
			node->line);
		goto exit;
	}
	key = avrule->src_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		avrule->src_str = __cil_attrib_get_versname(key, args->num);
	} else {
		avrule->src_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}
	key = avrule->tgt_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		avrule->tgt_str = __cil_attrib_get_versname(key, args->num);
	} else if (CIL_KEY_SELF != key) {
		avrule->tgt_str = __cil_attrib_get_versname(key, NON_PLAT_SUFFIX);
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_genfscon(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;

	struct cil_genfscon *genfscon = (struct cil_genfscon *)node->data;

	if (genfscon->context_str != NULL) {
		/* genfscon refers to a named context, which will be modified elsewhere */
	} else {
		/* genfscon contains an anon context, which needs to have type checked */
		rc = cil_attrib_check_context(genfscon->context, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int cil_attrib_fsuse(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_fsuse *fsuse = (struct cil_fsuse *)node->data;

	if (fsuse->context_str != NULL) {
		/* fsuse refers to a named context, which will be modified elsewhere */
	} else {
		/* fsuse contains an anon context, which needs to have type checked */
		rc = cil_attrib_check_context(fsuse->context, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;
exit:
	return rc;
}

static int __attributize_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args) {
	int rc = SEPOL_ERR;
	struct version_args *args = (struct version_args *) extra_args;

	if (node == NULL || finished == NULL || extra_args == NULL) {
		goto exit;
	}

	switch (node->flavor) {
	case CIL_SIDCONTEXT:
		/* contains type, but shouldn't involve an attributized type, maybe add
		   a check on type and error if it conflicts */
		rc = cil_attrib_sidcontext(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_ROLE:
		cil_log(CIL_ERR, "%s declaration illegal non-platform policy (line %d)\n",
			CIL_KEY_ROLE, node->line);
		rc = SEPOL_ERR;
		break;
	case CIL_ROLETYPE:
		/* Yes, this is needed if we support roletype in non-platform policy.
		   type_id can be type, typealias or typeattr */
		rc = cil_attrib_roletype(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_ROLEATTRIBUTE:
		/* don't think this is needed, only used for cil_gen_req, and we aren't
		   yet supporting roles in non-platform policy. */
		break;
	case CIL_TYPE:
		/* conver to attribute if in policy */
		rc = cil_attrib_type(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_TYPEPERMISSIVE:
		/* not sure how to handle this - throw error if targeting platform type.
		   Could maybe add support for permissive attributes. */
		rc = cil_attrib_typepermissive(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_TYPEATTRIBUTE:
		rc = cil_attrib_typeattribute(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_TYPEATTRIBUTESET:
		rc = cil_attrib_typeattributeset(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_TYPEALIASACTUAL:
		/* this will break on an attributized type - identify it and throw error */
		rc = cil_attrib_typealiasactual(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_NAMETYPETRANSITION:
		/* not allowed in plat-policy. Types present, throw error if attributee */
		rc = cil_attrib_nametypetransition(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_TYPE_RULE:
		/* not allowed in plat-policy. Types present, throw error if attributee */
		rc = cil_attrib_type_rule(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_AVRULE:
	case CIL_AVRULEX:
		rc = cil_attrib_avrule(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_CONTEXT:
		/* not currently found in AOSP policy, but if found would need to be
		   checked to not be attributee */
		rc = cil_attrib_context(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_GENFSCON:
		/* not allowed in plat-policy, but types present, throw error if attributee */
		rc = cil_attrib_genfscon(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_FILECON:
	case CIL_NODECON:
	case CIL_PORTCON:
	case CIL_PIRQCON:
	case CIL_IOMEMCON:
	case CIL_IOPORTCON:
	case CIL_PCIDEVICECON:
	case CIL_DEVICETREECON:
		/* TODO: not currently found in AOSP policy, but if found would need to be
		   checked to not be attributee */
		break;
	case CIL_FSUSE:
		/* not allowed in plat-policy, but types present, throw error if attributee */
		cil_attrib_fsuse(node, args);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	case CIL_CONSTRAIN:
	case CIL_MLSCONSTRAIN:
		/* there is type info here, but not sure if we'll allow non-platform code
		   to have this, or whether or not its in platform policy.  Currently
		   assuming that mlsconstrain is private-platform only, and that normal
		   constrain is verboten. */
		break;
	case CIL_VALIDATETRANS:
	case CIL_MLSVALIDATETRANS:
		/* not used in AOSP, but shouldn't be in platform */
		break;
	case CIL_CALL:
	case CIL_MACRO:
	case CIL_OPTIONAL:
		/* not used in AOSP, not considered for now */
		break;
	default:
		break;
	}

	return SEPOL_OK;
exit:
	return rc;
}

/*
 * walk ast, replacing previously identified types and attributes with the
 * attributized version. Also replace previous references to the attributees
 * with the versioned type.
 */
static int cil_attributize(struct cil_db *db, hashtab_t vers_map, const char *num) {
	int rc = SEPOL_ERR;
	struct version_args extra_args;
	extra_args.db = db;
	extra_args.vers_map = vers_map;
	extra_args.num = num;

	rc = cil_tree_walk(db->ast->root, __attributize_helper, NULL, NULL, &extra_args);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;
exit:
	return rc;
}

/*
 * Create typeattributeset mappings from the attributes generated from the
 * original types/attributes to the original values.  This mapping will provide
 * the basis for the platform policy's mapping to this public version.
 *
 * Add these new typeattributeset nodes to the given cil_db.
 */
static int cil_build_mappings_tree(hashtab_key_t k,
			    __attribute__((unused)) hashtab_datum_t d, void *args) {
	struct cil_typeattributeset *attrset = NULL;
	struct cil_tree_node *ast_node = NULL;
	struct version_datum *ver_datum = (struct version_datum *) d;
	struct version_args *verargs = (struct version_args *)args;
	struct cil_tree_node *ast_parent = verargs->db->ast->root;
	char *orig_type = (char *) k;
	char *vrsnd_type = __cil_attrib_get_versname(orig_type, verargs->num);

	/* create typeattributeset datum */
	cil_typeattributeset_init(&attrset);
	cil_list_init(&attrset->str_expr, CIL_TYPE);
	attrset->attr_str = vrsnd_type;
	cil_list_append(attrset->str_expr, CIL_STRING, orig_type);

	/* create containing tree node */
	cil_tree_node_init(&ast_node);
	ast_node->data = attrset;
	ast_node->flavor = CIL_TYPEATTRIBUTESET;

	/* add to tree */
	ast_node->parent = ast_parent;
	if (ast_parent->cl_head == NULL)
		ast_parent->cl_head = ast_node;
	else
		ast_parent->cl_tail->next = ast_node;
	ast_parent->cl_tail = ast_node;
	return SEPOL_OK;
}

/*
 * Initializes the given db and uses the version mapping generated by
 * cil_extract_attributees() to fill it with the glue policy required to
 * connect the attributized policy created by cil_attributize() to the policy
 * declaring the concrete types.
 */
int cil_attrib_mapping(struct cil_db **db, hashtab_t vers_map, const char *num) {
	int rc = SEPOL_ERR;
	struct version_args extra_args;

	cil_db_init(db);

	/* foreach entry in vers_map, create typeattributeset node and attach to tree */
	extra_args.db = *db;
	extra_args.vers_map = NULL;
	extra_args.num = num;
	rc = hashtab_map(vers_map, cil_build_mappings_tree, &extra_args);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;
exit:
	return rc;
}

int cil_android_attrib_mapping(struct cil_db **mdb, struct cil_db *srcdb, const char *num) {
	int rc = SEPOL_ERR;
	hashtab_t ver_map_tab = NULL;

	ver_map_tab = hashtab_create(ver_map_hash_val, ver_map_key_cmp, VER_MAP_SZ);
	if (!ver_map_tab) {
		cil_log(CIL_ERR, "Unable to create version mapping table.\n");
		goto exit;
	}
	rc = cil_build_ast(srcdb, srcdb->parse->root, srcdb->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Unable to build source db AST.\n");
		goto exit;
	}
	rc = cil_extract_attributees(srcdb, ver_map_tab);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Unable to extract attributizable elements from source db.\n");
		goto exit;
	}
	rc = cil_attrib_mapping(mdb, ver_map_tab, num);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Unable to create mapping db from source db.\n");
		goto exit;
	}
	rc = SEPOL_OK;
exit:
	ver_map_destroy(ver_map_tab);
	return rc;
}

int cil_android_attributize(struct cil_db *tgtdb, struct cil_db *srcdb, const char *num) {
	int rc = SEPOL_ERR;
	hashtab_t ver_map_tab = NULL;

	ver_map_tab = hashtab_create(ver_map_hash_val, ver_map_key_cmp, VER_MAP_SZ);
	if (!ver_map_tab) {
		cil_log(CIL_ERR, "Unable to create version mapping table.\n");
		goto exit;
	}
	rc = cil_build_ast(srcdb, srcdb->parse->root, srcdb->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Unable to build source db AST.\n");
		goto exit;
	}
	rc = cil_extract_attributees(srcdb, ver_map_tab);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Unable to extract attributizable elements from source db.\n");
		goto exit;
	}
	rc = cil_build_ast(tgtdb, tgtdb->parse->root, tgtdb->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Unable to build target db AST.\n");
		goto exit;
	}
	rc = cil_attributize(tgtdb, ver_map_tab, num);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Unable to attributize target db.\n");
		goto exit;
	}

	rc = SEPOL_OK;
exit:
	ver_map_destroy(ver_map_tab);
	return rc;
}

int __ns_process_line(FILE *out, char *line, hashtab_t ver_map_tab) {
	int rc = SEPOL_ERR;
	char *beg, *end, *ns_type, *type = NULL;;
	size_t type_sz;

	beg = end = strstr(line, ":s0");
	if (end == NULL)
		goto asis;

	/* find type border */
	while (beg != line) {
		if (*(--beg) == ':')
			break;
	}
	if (beg == line || beg == end) {
		goto asis;
	}
	beg++;
	type_sz = end - beg;
	type = cil_malloc(type_sz + 1);
	strncpy(type, beg, type_sz);
	type[type_sz] = '\0';
	if (hashtab_search(ver_map_tab, (hashtab_key_t) type) != NULL)
		goto asis;
	ns_type = __cil_attrib_get_versname(type, NON_PLAT_SUFFIX);
	while(line != beg) {
		if (fputc(*line++, out) < 0) {
			rc = SEPOL_ERR;
			goto exit;
		}
	}
	if (fputs(ns_type, out) < 0) {
		rc = SEPOL_ERR;
		goto exit;
	}
	if (fputs(end, out) < 0) {
		rc = SEPOL_ERR;
		goto exit;
	}
	rc = SEPOL_OK;
exit:
	free(type);
	return rc;
asis:
	if (fputs(line, out) < 0) {
		rc = SEPOL_ERR;
		goto exit;
	} else {
		rc = SEPOL_OK;
		goto exit;
	}
}

/*
 * TODO - move to libselinux labeling backends directly.  This is a hack that
 * should use the labeling backends in libselinux to concretely deal with
 * contexts and properly interact with the context source files.
 */
int cil_android_namespace_contexts(const char *ctxts_dest, const char *ctxts_src,
                                   struct cil_db *srcdb) {
	int rc = SEPOL_ERR;
	hashtab_t ver_map_tab = NULL;
	FILE *in = NULL, *out = NULL;
	size_t line_len;
	char *line_buf = NULL;

	ver_map_tab = hashtab_create(ver_map_hash_val, ver_map_key_cmp, VER_MAP_SZ);
	if (!ver_map_tab) {
		cil_log(CIL_ERR, "Unable to create version mapping table.\n");
		rc = SEPOL_ERR;
		goto exit;
	}
	rc = cil_build_ast(srcdb, srcdb->parse->root, srcdb->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Unable to build source db AST.\n");
		goto exit;
	}
	rc = cil_extract_attributees(srcdb, ver_map_tab);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Unable to extract attributizable elements from source db.\n");
		goto exit;
	}
	in = fopen(ctxts_src, "r");
	if (in == NULL) {
		cil_log(CIL_ERR, "Failure opening input contexts file for namespacing: %s\n",
                strerror(errno));
		rc = SEPOL_ERR;
		goto exit;
	}
	out = fopen(ctxts_dest, "w");
	if (out == NULL) {
		cil_log(CIL_ERR, "Failure opening output contexts file for namespacing: %s\n",
                strerror(errno));
		rc = SEPOL_ERR;
        fclose(in);
		goto exit;
	}
	/* process file and convert */
	while(getline(&line_buf, &line_len, in) > 0) {
		rc = __ns_process_line(out, line_buf, ver_map_tab);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}
    fclose(in);
	fclose(out);
exit:
	ver_map_destroy(ver_map_tab);
	free(line_buf);
	return rc;
}
