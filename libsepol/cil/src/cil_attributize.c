#include "cil_attributize.h"
#include "cil_version_map.h"

struct version_args {
    struct cil_db *db;
    hashtab_t vers_map;
    const char *num;
};

static int __extract_attributees_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args) {
	int rc = SEPOL_ERR;
	struct version_args *args = (struct version_args *) extra_args;
	char *key;
	struct version_datum *datum;

	if (node == NULL || finished == NULL || extra_args == NULL)
		return rc;

	switch (node->flavor) {
	case CIL_TYPE:
	case CIL_TYPEATTRIBUTE:
		datum = cil_malloc(sizeof(*datum));
		datum->db = args->db;
		datum->ast_node = node;
		datum->orig_name = ((struct cil_symtab_datum *)node->data)->name;
		key = datum->orig_name;
		if (strncmp(key, "base_typeattr_", 14)) {
			/* secilc creates base attributes which are just typeattributesets,
			 * of the existing types and attributes.  Ignore them. */
			rc = hashtab_insert(args->vers_map, (hashtab_key_t) key,
					    (hashtab_datum_t) datum);
		}
		break;
	case CIL_TYPEALIAS:
		cil_log(CIL_ERR, "%s should not be used in platform public policy (line %d)\n",
			CIL_KEY_TYPEALIAS, node->line);
		rc = SEPOL_ERR;
		break;
	case CIL_TYPEPERMISSIVE:
		cil_log(CIL_ERR, "%s should not be used in platform public policy (line %d)\n",
			CIL_KEY_TYPEPERMISSIVE, node->line);
		rc = SEPOL_ERR;
		break;
	default:
		rc = SEPOL_OK;
		break;
	}
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
	// walk ast.
	int rc = SEPOL_ERR;
	struct version_args extra_args;
	extra_args.db = db;
	extra_args.vers_map = vers_map;
	extra_args.num = NULL;
	rc = cil_tree_walk(db->ast->root, __extract_attributees_helper, NULL, NULL, &extra_args);
	return rc;
}

/*
 * Takes the old name and version string and creates a new strpool entry by
 * combining them.
 */
char *__cil_attrib_get_versname(char *old, const char *vers) {
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

int __cil_attrib_convert_type(struct cil_tree_node *node, struct version_args *args) {

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
	return rc;
}

/*
 * datum needs updating - create new key, remove entry under old key,
 * update entry, and insert under new key
 */
int __cil_attrib_swap_symtab_key(struct cil_tree_node *node, char *old_key,
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

	return rc;
}

/*
 * expressions may contains strings which are not in the type-attribute
 * namespace, so this is not a general cil_expr attributizer.
 * TODO: add support for other types of expressions which may contain types.
 */
int cil_attrib_type_expr(struct cil_list *expr_str, struct version_args *args) {
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
	rc = SEPOL_OK;
exit:
	return rc;
}

int cil_attrib_check_context(struct cil_context *ctxt, struct version_args *args) {
	int rc = SEPOL_ERR;

	if (ctxt->type != NULL) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported.\n");
		rc = SEPOL_ERR;
		goto exit;
	}
	if (hashtab_search(args->vers_map, (hashtab_key_t) ctxt->type_str) != NULL) {
		cil_log(CIL_ERR, "AST contains context with platform public type: %s\n",
			ctxt->type_str);
		rc = SEPOL_ERR;
		goto exit;
	}
	rc = SEPOL_OK;
exit:
	return rc;
}

int cil_attrib_sidcontext(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_sidcontext *sidcon = (struct cil_sidcontext *)node->data;

	if (sidcon->context_str != NULL) {
		/* sidcon refers to a named context, which will be modified elsewhere */
		rc = SEPOL_OK;
	} else {
		/* sidcon contains an anon context, which needs to have type checked */
		rc = cil_attrib_check_context(sidcon->context, args);
	}
	return rc;
}

int cil_attrib_context(struct cil_tree_node *node, struct version_args *args) {
	struct cil_context *ctxt = (struct cil_context *)node->data;

	return cil_attrib_check_context(ctxt, args);
}

int cil_attrib_roletype(__attribute__((unused)) struct cil_tree_node *node,
			__attribute__((unused)) struct version_args *args) {
	cil_log(CIL_ERR, "roletype statements not (yet) supported in non-platform policy\n");
	return SEPOL_ERR;
}

int cil_attrib_type(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_type *type = (struct cil_type *)node->data;
	char *key = type->datum.name;

	if (type->value) {
		cil_log(CIL_ERR, "AST already resolved.  !!! Not yet supported.\n");
		rc = SEPOL_ERR;
		goto exit;
	}
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		rc = __cil_attrib_convert_type(node, args);
		if (rc != SEPOL_OK)
			goto exit;
	}
	rc = SEPOL_OK;
exit:
	return rc;
}

int cil_attrib_typepermissive(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_typepermissive *typeperm = (struct cil_typepermissive *)node->data;

	if (typeperm->type != NULL) {
		cil_log(CIL_ERR, "AST already resolved.  ### Not yet supported.\n");
		rc = SEPOL_ERR;
		goto exit;
	}
	if (hashtab_search(args->vers_map, (hashtab_key_t) typeperm->type_str) != NULL) {
		cil_log(CIL_ERR, "%s contains platform public type: %s (line %d) .\n",
			CIL_KEY_TYPEPERMISSIVE, typeperm->type_str, node->line);
		rc = SEPOL_ERR;
		goto exit;
	}
	rc = SEPOL_OK;
exit:
	return rc;
}

int cil_attrib_typeattribute(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	struct cil_typeattribute *typeattr = (struct cil_typeattribute *)node->data;
	char *key = typeattr->datum.name;

	if (typeattr->types) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported (line %d).\n",
			node->line);
		rc = SEPOL_ERR;
		goto exit;
	}
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		rc = __cil_attrib_swap_symtab_key(node, key, args->num);
		if (rc != SEPOL_OK)
			goto exit;
	}
	rc = SEPOL_OK;
exit:
	return rc;
}

int cil_attrib_typeattributeset(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;
	struct cil_typeattributeset *typeattrset = (struct cil_typeattributeset *) node->data;

	if (typeattrset->datum_expr != NULL) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported (line %d).\n",
			node->line);
		rc = SEPOL_ERR;
		goto exit;
	}

	key = typeattrset->attr_str;
	/* first check to see if the attribute to which this set belongs is versioned */
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		typeattrset->attr_str = __cil_attrib_get_versname(key, args->num);
	}

	rc = cil_attrib_type_expr(typeattrset->str_expr, args);
	if (rc != SEPOL_OK)
		goto exit;
	rc = SEPOL_OK;
exit:
	return rc;
}

int cil_attrib_typealiasactual(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_OK;
	char *key;
	struct cil_aliasactual *aliasact = (struct cil_aliasactual *)node->data;
	key = aliasact->actual_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		cil_log(CIL_ERR, "%s with platform public type not allowed (line %d)\n",
		    CIL_KEY_TYPEALIASACTUAL, node->line);
		rc = SEPOL_ERR;
	}
	return rc;
}

int cil_attrib_nametypetransition(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;
	struct cil_nametypetransition *namettrans = (struct cil_nametypetransition *)node->data;

	if (namettrans->src != NULL) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported (line %d).\n",
			node->line);
		rc = SEPOL_ERR;
		goto exit;
	}
	key = namettrans->result;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		cil_log(CIL_ERR, "%s with platform public default not allowed (line %d)\n",
		    CIL_KEY_TYPETRANSITION, node->line);
		rc = SEPOL_ERR;
		goto exit;
	}

	key = namettrans->src_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		namettrans->src_str = __cil_attrib_get_versname(key, args->num);
	}

	key = namettrans->tgt_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		namettrans->tgt_str = __cil_attrib_get_versname(key, args->num);
	}
	rc = SEPOL_OK;
exit:
	return rc;
}

/*
 * This is exactly the same as cil_attrib_nametypetransition, but the struct
 * layouts differ, so we can't reuse it.
 */
int cil_attrib_type_rule(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;
	struct cil_type_rule *type_rule = (struct cil_type_rule *)node->data;

	if (type_rule->src != NULL) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported (line %d).\n",
			node->line);
		rc = SEPOL_ERR;
		goto exit;
	}
	key = type_rule->result;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		cil_log(CIL_ERR, "%s with platform public default not allowed (line %d)\n",
		    CIL_KEY_TYPETRANSITION, node->line);
		rc = SEPOL_ERR;
		goto exit;
	}

	key = type_rule->src_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		type_rule->src_str = __cil_attrib_get_versname(key, args->num);
	}

	key = type_rule->tgt_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		type_rule->tgt_str = __cil_attrib_get_versname(key, args->num);
	}
	rc = SEPOL_OK;
exit:
	return rc;
}

int cil_attrib_avrule(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_ERR;
	char *key;
	struct cil_avrule *avrule = (struct cil_avrule *)node->data;

	if (avrule->src != NULL) {
		cil_log(CIL_ERR, "AST already resolved. Not yet supported (line %d).\n",
			node->line);
		rc = SEPOL_ERR;
		goto exit;
	}

	key = avrule->src_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		avrule->src_str = __cil_attrib_get_versname(key, args->num);
	}

	key = avrule->tgt_str;
	if (hashtab_search(args->vers_map, (hashtab_key_t) key) != NULL) {
		avrule->tgt_str = __cil_attrib_get_versname(key, args->num);
	}
	rc = SEPOL_OK;
exit:
	return rc;
}

int cil_attrib_genfscon(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_OK;

	struct cil_genfscon *genfscon = (struct cil_genfscon *)node->data;

	if (genfscon->context_str != NULL) {
		/* genfscon refers to a named context, which will be modified elsewhere */
		rc = SEPOL_OK;
	} else {
		/* genfscon contains an anon context, which needs to have type checked */
		rc = cil_attrib_check_context(genfscon->context, args);
	}
	return rc;
}

int cil_attrib_fsuse(struct cil_tree_node *node, struct version_args *args) {
	int rc = SEPOL_OK;

	struct cil_fsuse *fsuse = (struct cil_fsuse *)node->data;

	if (fsuse->context_str != NULL) {
		/* fsuse refers to a named context, which will be modified elsewhere */
		rc = SEPOL_OK;
	} else {
		/* fsuse contains an anon context, which needs to have type checked */
		rc = cil_attrib_check_context(fsuse->context, args);
	}
	return rc;
}

static int __attributize_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args) {
	int rc = SEPOL_OK;
	struct version_args *args = (struct version_args *) extra_args;

	if (node == NULL || finished == NULL || extra_args == NULL)
		return rc;

	switch (node->flavor) {
	case CIL_SIDCONTEXT:
		/* contains type, but shouldn't involve an attributized type, maybe add
		 * a check on type and error if it conflicts */
		rc = cil_attrib_sidcontext(node, args);
		break;
	case CIL_ROLETYPE:
		/* Yes, this is needed if we support roletype in non-platform policy.
		   type_id can be type, typealias or typeattr */
		rc = cil_attrib_roletype(node, args);
		break;
	case CIL_ROLEATTRIBUTE:
		/* don't think this is needed, only used for cil_gen_req, and we aren't
		 * yet supporting roles in non-platform policy. */
		break;
	case CIL_TYPE:
		/* conver to attribute if in policy */
		rc = cil_attrib_type(node, args);
		break;
	case CIL_TYPEPERMISSIVE:
		/* not sure how to handle this - throw error if targeting platform type.
		 * Could maybe add support for permissive attributes. */
		rc = cil_attrib_typepermissive(node, args);
		break;
	case CIL_TYPEATTRIBUTE:
		rc = cil_attrib_typeattribute(node, args);
		break;
	case CIL_TYPEATTRIBUTESET:
		rc = cil_attrib_typeattributeset(node, args);
		break;
	case CIL_TYPEALIASACTUAL:
		/* this will break on an attributized type - identify it and throw error */
		rc = cil_attrib_typealiasactual(node, args);
		break;
	case CIL_NAMETYPETRANSITION:
		/* not allowed in plat-policy. Types present, throw error if attributee */
		rc = cil_attrib_nametypetransition(node, args);
		break;
	case CIL_TYPE_RULE:
		/* not allowed in plat-policy. Types present, throw error if attributee */
		rc = cil_attrib_type_rule(node, args);
		break;
	case CIL_AVRULE:
	case CIL_AVRULEX:
		rc = cil_attrib_avrule(node, args);
		break;
	case CIL_CONTEXT:
		/* not currently found in AOSP policy, but if found would need to be
		 * checked to not be attributee*/
		rc = cil_attrib_context(node, args);
		break;
	case CIL_GENFSCON:
		/* not allowed in plat-policy, but types present, throw error if attributee */
		rc = cil_attrib_genfscon(node, args);
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
		 * checked to not be attributee*/
		break;
	case CIL_FSUSE:
		/* not allowed in plat-policy, but types present, throw error if attributee */
		cil_attrib_fsuse(node, args);
		break;
	case CIL_CONSTRAIN:
	case CIL_MLSCONSTRAIN:
		/* there is type info here, but not sure if we'll allow non-platform code
         * to have this, or whether or not its in platform policy.  Currently
		 * assuming that mlsconstrain is private-platform only, and that normal
		 * constrain is verboten. */
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
	return rc;
}

/* walk ast, replacing previously identified types and attributes with the
 * attributized version. Also replace previous references to the attributees
 * with the versioned type.
 */
int cil_attributize(struct cil_db *db, hashtab_t vers_map, const char *num) {

    int rc = SEPOL_ERR;
    struct version_args extra_args;
    extra_args.db = db;
    extra_args.vers_map = vers_map;
    extra_args.num = num;
    rc = cil_tree_walk(db->ast->root, __attributize_helper, NULL, NULL, &extra_args);
    return rc;
}
