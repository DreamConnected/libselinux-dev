<<<<<<< HEAD   (2ffdad Merge "Remove setools prebuilts")
#include <stdlib.h>

#include "cil_flavor.h"
#include "cil_internal.h"
#include "cil_log.h"
#include "cil_tree.h"

struct cil_args_write {
	FILE *cil_out;
	struct cil_db *db;
};

static int cil_unfill_expr(struct cil_list *expr_str, char **out_str, int paren);
static int cil_unfill_classperms_list(struct cil_list *classperms, char **out_str, int paren);
static int __cil_write_first_child_helper(struct cil_tree_node *node, void *extra_args);
static int __cil_write_node_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args);
static int __cil_write_last_child_helper(struct cil_tree_node *node, void *extra_args);

static int __cil_strlist_concat(struct cil_list *str_list, char **out_str, int paren) {
	size_t len = paren ? 3 : 1;
	size_t num_elems = 0;
	char *p = NULL;
	struct cil_list_item *curr;

	/* get buffer size */
	cil_list_for_each(curr, str_list) {
		len += strlen((char *)curr->data);
		num_elems++;
	}
	if (num_elems != 0) {
		/* add spaces between elements */
		len += num_elems - 1;
	}
	*out_str = cil_malloc(len);
	p = *out_str;
	if (paren)
		*p++ = '(';
	cil_list_for_each(curr, str_list) {
		size_t src_len = strlen((char *)curr->data);
		memcpy(p, curr->data, src_len);
		p += src_len;
		if (curr->next != NULL)
			*p++ = ' ';
	}
	if (paren)
		*p++ = ')';
	*p++ = '\0';
	return SEPOL_OK;
}

static int __cil_unfill_expr_helper(struct cil_list_item *curr,
			     struct cil_list_item **next, char **out_str, int paren) {
	int rc = SEPOL_ERR;
	char *str = NULL;
	char *operand1 = NULL;
	char *operand2 = NULL;

	switch(curr->flavor) {
	case CIL_LIST:
		rc = cil_unfill_expr((struct cil_list *)curr->data, &str, paren);
		if (rc != SEPOL_OK)
			goto exit;
		*out_str = str;
		*next = curr->next;
		break;
	case CIL_STRING:
		str = strdup((char *)curr->data);
		if (!str) {
			cil_log(CIL_ERR, "OOM. Unable to copy string.\n");
            rc = SEPOL_ERR;
			goto exit;
		}
		*out_str = str;
		*next = curr->next;
		break;
	case CIL_DATUM:
		str = strdup(((struct cil_symtab_datum *)curr->data)->name);
		if (!str) {
			cil_log(CIL_ERR, "OOM. Unable to copy string.\n");
            rc = SEPOL_ERR;
			goto exit;
		}
		*out_str = str;
		*next = curr->next;
		break;
	case CIL_OP: {
		char *op_str = NULL;
		size_t len = 0;
		enum cil_flavor op_flavor = (enum cil_flavor)curr->data;
		switch (op_flavor) {
		case CIL_AND:
			op_str = CIL_KEY_AND;
			break;
		case CIL_OR:
			op_str = CIL_KEY_OR;
			break;
		case CIL_NOT:
			op_str = CIL_KEY_NOT;
			break;
		case CIL_ALL:
			op_str = CIL_KEY_ALL;
			break;
		case CIL_EQ:
			op_str = CIL_KEY_EQ;
			break;
		case CIL_NEQ:
			op_str = CIL_KEY_NEQ;
			break;
		case CIL_RANGE:
			op_str = CIL_KEY_RANGE;
			break;
		case CIL_XOR:
			op_str = CIL_KEY_XOR;
			break;
		case CIL_CONS_DOM:
			op_str = CIL_KEY_CONS_DOM;
			break;
		case CIL_CONS_DOMBY:
			op_str = CIL_KEY_CONS_DOMBY;
			break;
		case CIL_CONS_INCOMP:
			op_str = CIL_KEY_CONS_INCOMP;
			break;
		default:
			cil_log(CIL_ERR, "Unknown operator in expression: %d\n", op_flavor);
			goto exit;
			break;
		}
		/* all operands take two args except for 'all' and 'not', which take
		 * one and two, respectively */
		len = strlen(op_str) + 3;
		if (op_flavor == CIL_ALL) {
			*out_str = cil_malloc(len);
			sprintf(*out_str, "(%s)", op_str);
			*next = curr->next;
		} else if (op_flavor == CIL_NOT) {
			rc = __cil_unfill_expr_helper(curr->next, next, &operand1, paren);
			if (rc != SEPOL_OK)
				goto exit;
			len += strlen(operand1) + 1;
			*out_str = cil_malloc(len);
			sprintf(*out_str, "(%s %s)", op_str, operand1);
			// *next already set by recursive call
		} else {
			rc = __cil_unfill_expr_helper(curr->next, next, &operand1, paren);
			if (rc != SEPOL_OK)
				goto exit;
			len += strlen(operand1) + 1;
			// *next contains operand2, but keep track of next after that
			rc = __cil_unfill_expr_helper(*next, next, &operand2, paren);
			if (rc != SEPOL_OK)
				goto exit;
			len += strlen(operand2) + 1;
			*out_str = cil_malloc(len);
			sprintf(*out_str, "(%s %s %s)", op_str, operand1, operand2);
			// *next already set by recursive call
		}
	}
		break;
	case CIL_CONS_OPERAND: {
		enum cil_flavor operand_flavor = (enum cil_flavor)curr->data;
		char *operand_str = NULL;
		switch (operand_flavor) {
		case CIL_CONS_U1:
			operand_str = CIL_KEY_CONS_U1;
			break;
		case CIL_CONS_U2:
			operand_str = CIL_KEY_CONS_U2;
			break;
		case CIL_CONS_U3:
			operand_str = CIL_KEY_CONS_U3;
			break;
		case CIL_CONS_T1:
			operand_str = CIL_KEY_CONS_T1;
			break;
		case CIL_CONS_T2:
			operand_str = CIL_KEY_CONS_T2;
			break;
		case CIL_CONS_T3:
			operand_str = CIL_KEY_CONS_T3;
			break;
		case CIL_CONS_R1:
			operand_str = CIL_KEY_CONS_R1;
			break;
		case CIL_CONS_R2:
			operand_str = CIL_KEY_CONS_R2;
			break;
		case CIL_CONS_R3:
			operand_str = CIL_KEY_CONS_R3;
			break;
		case CIL_CONS_L1:
			operand_str = CIL_KEY_CONS_L1;
			break;
		case CIL_CONS_L2:
			operand_str = CIL_KEY_CONS_L2;
			break;
		case CIL_CONS_H1:
			operand_str = CIL_KEY_CONS_H1;
			break;
		case CIL_CONS_H2:
			operand_str = CIL_KEY_CONS_H2;
			break;
		default:
			cil_log(CIL_ERR, "Unknown operand in expression\n");
			goto exit;
			break;
		}
		str = strdup(operand_str);
		if (!str) {
			cil_log(CIL_ERR, "OOM. Unable to copy string.\n");
            rc = SEPOL_ERR;
			goto exit;
		}
		*out_str = str;
		*next = curr->next;
	}
		break;
	default:
		cil_log(CIL_ERR, "Unknown flavor in expression\n");
		goto exit;
		break;
	}
	rc = SEPOL_OK;
exit:
	free(operand1);
	free(operand2);
	return rc;
}

static int cil_unfill_expr(struct cil_list *expr_str, char **out_str, int paren) {
	int rc = SEPOL_ERR;

	/* reuse cil_list to keep track of strings */
	struct cil_list *str_list = NULL;
	struct cil_list_item *curr = NULL;

	cil_list_init(&str_list, CIL_NONE);

	/* iterate through cil_list, grabbing elements as needed */
	curr = expr_str->head;
	while(curr != NULL) {
		char *str = NULL;
		struct cil_list_item *next = NULL;

		rc = __cil_unfill_expr_helper(curr, &next, &str, paren);
        if (rc != SEPOL_OK)
            goto exit;
		cil_list_append(str_list, CIL_STRING, (void *) str);
		str = NULL;
		curr = next;
	}
	rc = __cil_strlist_concat(str_list, out_str, paren);
	if (rc != SEPOL_OK)
		goto exit;
	rc = SEPOL_OK;
exit:
	cil_list_for_each(curr, str_list) {
		free(curr->data);
	}
	cil_list_destroy(&str_list, 0);
	return rc;
}

static int cil_unfill_cats(struct cil_cats *cats, char **out_str) {
	return cil_unfill_expr(cats->str_expr, out_str, 0);
}

static int cil_unfill_level(struct cil_level *lvl, char **out_str) {
	int rc = SEPOL_ERR;
	size_t len = 0;
	char *sens, *cats = NULL;
	sens = lvl->sens_str;
	len = strlen(sens) + 3; // '()\0'
	if (lvl->cats != NULL) {
		rc = cil_unfill_cats(lvl->cats, &cats);
		if (rc != SEPOL_OK)
			goto exit;
		len += strlen(cats) + 1;
	}
	*out_str = cil_malloc(len);
	if (cats == NULL) {
		if (sprintf(*out_str, "(%s)", sens) < 0) {
			cil_log(CIL_ERR, "Error unpacking and writing level\n");
			rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		if (sprintf(*out_str, "(%s %s)", sens, cats) < 0) {
			cil_log(CIL_ERR, "Error unpacking and writing level\n");
			rc = SEPOL_ERR;
			goto exit;
		}
	}
	rc = SEPOL_OK;
exit:
	free(cats);
	return rc;
}

static int cil_unfill_levelrange(struct cil_levelrange *lvlrnge, char **out_str) {
	int rc = SEPOL_ERR;
	size_t len = 0;
	char *low = NULL, *high = NULL;
	if (lvlrnge->low_str != NULL) {
		low = strdup(lvlrnge->low_str);
		if (low == NULL) {
			cil_log(CIL_ERR, "OOM. Unable to copy level string.\n");
            rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		rc = cil_unfill_level(lvlrnge->low, &low);
		if (rc != SEPOL_OK)
			goto exit;
	}
	if (lvlrnge->high_str != NULL) {
		high = strdup(lvlrnge->high_str);
		if (high == NULL) {
			cil_log(CIL_ERR, "OOM. Unable to copy level string.\n");
            rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		rc = cil_unfill_level(lvlrnge->high, &high);
		if (rc != SEPOL_OK)
			goto exit;
	}
	len = strlen(low) + strlen(high) + 4;
	*out_str = cil_malloc(len);
	if (sprintf(*out_str, "(%s %s)", low, high) < 0) {
		cil_log(CIL_ERR, "Error unpacking and writing levelrange\n");
		rc = SEPOL_ERR;
		goto exit;
	}
	rc = SEPOL_OK;
exit:
	free(low);
	free(high);
	return rc;
}

static int cil_unfill_context(struct cil_context *context, char **out_str) {
	int rc = SEPOL_ERR;
	size_t len = 0;
	char *user_str, *role_str, *type_str;
	char *range_str = NULL;

	user_str = context->user_str;
	role_str = context->role_str;
	type_str = context->type_str;
	if (context->range_str != NULL) {
		range_str = strdup(context->range_str);
		if (range_str == NULL) {
			cil_log(CIL_ERR, "OOM. Unable to copy range string.\n");
            rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		rc = cil_unfill_levelrange(context->range, &range_str);
		if (rc != SEPOL_OK)
			goto exit;
	}
	len = strlen(user_str) + strlen(role_str) + strlen(type_str)
		+ strlen(range_str) + 6;
	*out_str = cil_malloc(len);
	if (sprintf(*out_str, "(%s %s %s %s)", user_str, role_str, type_str, range_str) < 0) {
		cil_log(CIL_ERR, "Error unpacking and writing context\n");
		rc = SEPOL_ERR;
		goto exit;
	}
	rc = SEPOL_OK;
exit:
	free(range_str);
	return rc;
}

static int cil_unfill_permx(struct cil_permissionx *permx, char **out_str) {
	size_t len = 3;
	int rc = SEPOL_ERR;
	char *kind, *obj;
	char *expr = NULL;

	switch (permx->kind) {
	case CIL_PERMX_KIND_IOCTL:
		kind = CIL_KEY_IOCTL;
		break;
	default:
		cil_log(CIL_ERR, "Unknown permissionx kind: %d\n", permx->kind);
		rc = SEPOL_ERR;
		goto exit;
		break;
	}
	obj = permx->obj_str;
	rc = cil_unfill_expr(permx->expr_str, &expr, 1);
	if (rc != SEPOL_OK)
		goto exit;
	len += strlen(kind) + strlen(obj) + strlen(expr) + 2;
	*out_str = cil_malloc(len);
	if (sprintf(*out_str, "(%s %s %s)", kind, obj, expr) < 0) {
		cil_log(CIL_ERR, "Error writing xperm\n");
		rc = SEPOL_ERR;
		goto exit;
	}
	rc = SEPOL_OK;
exit:
	free(expr);
	return rc;
}

#define cil_write_unsupported(flavor) _cil_write_unsupported(flavor, __LINE__)
static int _cil_write_unsupported(const char *flavor, int line) {
	cil_log(CIL_ERR,
			"flavor \"%s\" is not supported, look in file \"%s\""
			" on line %d to add support.\n", flavor, __FILE__, line);
	return SEPOL_ENOTSUP;
}

static int cil_write_policycap(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_policycap *polcap = (struct cil_policycap *)node->data;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_POLICYCAP, polcap->datum.name);
	return SEPOL_OK;
}

static int cil_write_perm(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_perm *perm = (struct cil_perm *)node->data;
	fprintf(cil_out, "%s", perm->datum.name);
	if (node->next != NULL)
		fprintf(cil_out, " ");
	return SEPOL_OK;
}


static int cil_write_class(struct cil_tree_node *node, uint32_t *finished,
		     struct cil_args_write *extra_args) {
	int rc = SEPOL_ERR;
	FILE *cil_out = extra_args->cil_out;
	struct cil_symtab_datum *datum = (struct cil_symtab_datum *)node->data;
	char *class_type = (node->flavor == CIL_CLASS) ? CIL_KEY_CLASS : CIL_KEY_COMMON;

	/* print preamble */
	fprintf(cil_out, "(%s %s ", class_type, datum->name);

	if (node->cl_head == NULL) {
		/* no associated perms in this part of tree */
		fprintf(cil_out, "()");
	} else {

		/* visit subtree (perms) */
		rc = cil_tree_walk(node, __cil_write_node_helper,
				   __cil_write_first_child_helper,
				   __cil_write_last_child_helper,
				   extra_args);
		if (rc != SEPOL_OK)
			goto exit;
	}

	/* postamble (trailing paren) */
	fprintf(cil_out, ")\n");
	*finished = CIL_TREE_SKIP_HEAD;
	rc = SEPOL_OK;
exit:
	return rc;
}

static int cil_write_classorder(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *ord_str = NULL;
	struct cil_classorder *classord = (struct cil_classorder *)node->data;

	/* cil_unfill_expr() has logic to stringify a cil_list, reuse that. */
	rc = cil_unfill_expr(classord->class_list_str, &ord_str, 1);
	if (rc != SEPOL_OK)
		goto exit;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_CLASSORDER, ord_str);
	rc = SEPOL_OK;
exit:
	free(ord_str);
	return rc;
}

static int cil_write_classcommon(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_classcommon *classcommon = (struct cil_classcommon *)node->data;
	fprintf(cil_out, "(%s %s %s)\n", CIL_KEY_CLASSCOMMON, classcommon->class_str,
		classcommon->common_str);
	return SEPOL_OK;
}

static int cil_write_sid(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_sid *sid = (struct cil_sid *)node->data;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_SID, sid->datum.name);
	return SEPOL_OK;
}

static int cil_write_sidcontext(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *sid;
	char *ctx_str = NULL;
	struct cil_sidcontext *sidcon = (struct cil_sidcontext *)node->data;

	sid = sidcon->sid_str;
	if (sidcon->context_str != NULL) {
		ctx_str = strdup(sidcon->context_str);
		if (ctx_str == NULL) {
			cil_log(CIL_ERR, "OOM. Unable to copy context string.\n");
            rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		rc = cil_unfill_context(sidcon->context, &ctx_str);
		if (rc != SEPOL_OK)
			goto exit;
	}
	fprintf(cil_out, "(%s %s %s)\n", CIL_KEY_SIDCONTEXT, sid, ctx_str);
	rc = SEPOL_OK;
exit:
	free(ctx_str);
	return rc;
}

static int cil_write_sidorder(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *ord_str = NULL;
	struct cil_sidorder *sidord = (struct cil_sidorder *)node->data;

	/* cil_unfill_expr() has logic to stringify a cil_list, reuse that. */
	rc = cil_unfill_expr(sidord->sid_list_str, &ord_str, 1);
	if (rc != SEPOL_OK)
		goto exit;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_SIDORDER, ord_str);
	rc = SEPOL_OK;
exit:
	free(ord_str);
	return rc;
}

static int cil_write_user(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_user *user = (struct cil_user *)node->data;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_USER, user->datum.name);
	return SEPOL_OK;
}

static int cil_write_userrole(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_userrole *userrole = (struct cil_userrole *)node->data;
	fprintf(cil_out, "(%s %s %s)\n", CIL_KEY_USERROLE, userrole->user_str,
		userrole->role_str);
	return SEPOL_OK;
}

static int cil_write_userlevel(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_userlevel *usrlvl = (struct cil_userlevel *)node->data;
	int rc = SEPOL_ERR;
	char *usr;
	char *lvl = NULL;

	usr = usrlvl->user_str;
	if (usrlvl->level_str != NULL) {
		lvl = strdup(usrlvl->level_str);
		if (lvl == NULL) {
			cil_log(CIL_ERR, "OOM. Unable to copy level string.\n");
            rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		rc = cil_unfill_level(usrlvl->level, &lvl);
		if (rc != SEPOL_OK)
			goto exit;
	}
	fprintf(cil_out, "(%s %s %s)\n", CIL_KEY_USERLEVEL, usr, lvl);
	rc = SEPOL_OK;
exit:
	free(lvl);
	return rc;
}

static int cil_write_userrange(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_userrange *usrrng = (struct cil_userrange *)node->data;
	int rc = SEPOL_ERR;
	char *usr;
	char *range = NULL;

	usr = usrrng->user_str;
	if (usrrng->range_str != NULL) {
		range = strdup(usrrng->range_str);
		if (range == NULL) {
			cil_log(CIL_ERR, "OOM. Unable to copy levelrange string.\n");
            rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		rc = cil_unfill_levelrange(usrrng->range, &range);
		if (rc != SEPOL_OK)
			goto exit;
	}
	fprintf(cil_out, "(%s %s %s)\n", CIL_KEY_USERRANGE, usr, range);
	rc = SEPOL_OK;
exit:
	free(range);
	return rc;
}

static int cil_write_role(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_role *role = (struct cil_role *)node->data;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_ROLE, role->datum.name);
	return SEPOL_OK;
}

static int cil_write_roletype(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_roletype *roletype = (struct cil_roletype *)node->data;
	fprintf(cil_out, "(%s %s %s)\n", CIL_KEY_ROLETYPE, roletype->role_str, roletype->type_str);
	return SEPOL_OK;
}

static int cil_write_roleattribute(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_roleattribute *roleattr = (struct cil_roleattribute *)node->data;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_ROLEATTRIBUTE, roleattr->datum.name);
	return SEPOL_OK;
}

static int cil_write_type(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_type *type = (struct cil_type *)node->data;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_TYPE, type->datum.name);
	return SEPOL_OK;
}

static int cil_write_typepermissive(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_typepermissive *type = (struct cil_typepermissive *)node->data;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_TYPEPERMISSIVE, type->type_str);
	return SEPOL_OK;
}

static int cil_write_typeattribute(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_typeattribute *typeattr = (struct cil_typeattribute *)node->data;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_TYPEATTRIBUTE, typeattr->datum.name);
	return SEPOL_OK;
}

static int cil_write_typeattributeset(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *typeattr;
	char *set_str = NULL;
	struct cil_typeattributeset *typeattrset = (struct cil_typeattributeset *)node->data;

	typeattr = typeattrset->attr_str;
	rc = cil_unfill_expr(typeattrset->str_expr, &set_str, 1);
	if (rc != SEPOL_OK)
		goto exit;

	fprintf(cil_out, "(%s %s %s)\n", CIL_KEY_TYPEATTRIBUTESET, typeattr, set_str);
	rc = SEPOL_OK;
exit:
	free(set_str);
	return rc;
}

static int cil_write_expandtypeattribute(struct cil_tree_node *node, FILE *cil_out)
{
	int rc = SEPOL_ERR;
	char *attr_strs = NULL;
	struct cil_expandtypeattribute *expandattr = (struct cil_expandtypeattribute *)node->data;

	rc = cil_unfill_expr(expandattr->attr_strs, &attr_strs, 1);
	if (rc != SEPOL_OK)
		goto exit;

	fprintf(cil_out, "(%s %s %s)\n", CIL_KEY_EXPANDTYPEATTRIBUTE, attr_strs,
		expandattr->expand ? CIL_KEY_CONDTRUE : CIL_KEY_CONDFALSE);
	rc = SEPOL_OK;
exit:
	free(attr_strs);
	return rc;
}

static int cil_write_alias(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *type;
	struct cil_alias *alias = (struct cil_alias *)node->data;

	switch (node->flavor) {
	case CIL_TYPEALIAS:
		type = CIL_KEY_TYPEALIAS;
		break;
	case CIL_SENSALIAS:
		type = CIL_KEY_SENSALIAS;
		break;
	case CIL_CATALIAS:
		type = CIL_KEY_CATALIAS;
		break;
	default:
		cil_log(CIL_ERR, "Unknown alias type: %d\n", node->flavor);
		rc = SEPOL_ERR;
		goto exit;
		break;
	}
	fprintf(cil_out, "(%s %s)\n", type, alias->datum.name);
	rc = SEPOL_OK;
exit:
	return rc;
}

static int cil_write_aliasactual(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *type, *alias, *actual;
	struct cil_aliasactual *aliasact = (struct cil_aliasactual *)node->data;

	switch (node->flavor) {
	case CIL_TYPEALIASACTUAL:
		type = CIL_KEY_TYPEALIASACTUAL;
		break;
	case CIL_SENSALIASACTUAL:
		type = CIL_KEY_SENSALIASACTUAL;
		break;
	case CIL_CATALIASACTUAL:
		type = CIL_KEY_CATALIASACTUAL;
		break;
	default:
		cil_log(CIL_ERR, "Unknown alias type: %d\n", node->flavor);
		rc = SEPOL_ERR;
		goto exit;
		break;
	}
	alias = aliasact->alias_str;
	actual = aliasact->actual_str;
	fprintf(cil_out, "(%s %s %s)\n", type, alias, actual);
	rc = SEPOL_OK;
exit:
	return rc;
}

static int cil_write_nametypetransition(struct cil_tree_node *node, FILE *cil_out) {
	char *src, *tgt, *obj, *res, *name;
	struct cil_nametypetransition *ntrans = (struct cil_nametypetransition *)node->data;

	src = ntrans->src_str;
	tgt = ntrans->tgt_str;
	obj = ntrans->obj_str;
	res = ntrans->result_str;
	name = ntrans->name_str;
	fprintf(cil_out, "(%s %s %s %s \"%s\" %s)\n", CIL_KEY_TYPETRANSITION,
		src, tgt, obj, name, res);
	return SEPOL_OK;
}

static int cil_write_avrule_x(struct cil_avrule *avrule, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *rulekind, *src, *tgt;
	char *xperms = NULL;

	switch (avrule->rule_kind) {
	case CIL_AVRULE_ALLOWED:
		rulekind = CIL_KEY_ALLOWX;
		break;
	case CIL_AVRULE_AUDITALLOW:
		rulekind = CIL_KEY_AUDITALLOWX;
		break;
	case CIL_AVRULE_DONTAUDIT:
		rulekind = CIL_KEY_DONTAUDITX;
		break;
	case CIL_AVRULE_NEVERALLOW:
		rulekind = CIL_KEY_NEVERALLOWX;
		break;
	default:
		cil_log(CIL_ERR, "Unknown AVRULE type: %d\n", avrule->rule_kind);
		rc = SEPOL_ERR;
		goto exit;
		break;
	}
	src = avrule->src_str;
	tgt = avrule->tgt_str;

	if (avrule->perms.x.permx_str != NULL) {
		xperms = strdup(avrule->perms.x.permx_str);
		if (xperms == NULL) {
			cil_log(CIL_ERR, "OOM. Unable to copy xperms string.\n");
			rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		rc = cil_unfill_permx(avrule->perms.x.permx, &xperms);
		if (rc != SEPOL_OK)
			goto exit;
	}
	fprintf(cil_out, "(%s %s %s %s)\n", rulekind, src, tgt, xperms);
	rc = SEPOL_OK;
exit:
	free(xperms);
	return rc;
}

static int cil_write_avrule_orig(struct cil_avrule *avrule, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *rulekind, *src, *tgt;
	char *classperms = NULL;

	switch (avrule->rule_kind) {
	case CIL_AVRULE_ALLOWED:
		rulekind = CIL_KEY_ALLOW;
		break;
	case CIL_AVRULE_AUDITALLOW:
		rulekind = CIL_KEY_AUDITALLOW;
		break;
	case CIL_AVRULE_DONTAUDIT:
		rulekind = CIL_KEY_DONTAUDIT;
		break;
	case CIL_AVRULE_NEVERALLOW:
		rulekind = CIL_KEY_NEVERALLOW;
		break;
	default:
		cil_log(CIL_ERR, "Unknown AVRULE type: %d\n", avrule->rule_kind);
		rc = SEPOL_ERR;
		goto exit;
		break;
	}
	src = avrule->src_str;
	tgt = avrule->tgt_str;

	rc = cil_unfill_classperms_list(avrule->perms.classperms, &classperms, 0);
	if (rc != SEPOL_OK)
		goto exit;
	fprintf(cil_out, "(%s %s %s %s)\n", rulekind, src, tgt, classperms);
	rc = SEPOL_OK;
exit:
	free(classperms);
	return rc;
}

static int cil_write_avrule(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	struct cil_avrule *avrule = (struct cil_avrule *)node->data;

	if (avrule->is_extended)
		rc = cil_write_avrule_x(avrule, cil_out);
	else
		rc = cil_write_avrule_orig(avrule, cil_out);
	return rc;
}

static int cil_write_type_rule(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *type, *src, *tgt, *obj, *res;
	struct cil_type_rule *typerule = (struct cil_type_rule *)node->data;

	switch (typerule->rule_kind) {
	case CIL_TYPE_TRANSITION:
		type = CIL_KEY_TYPETRANSITION;
		break;
	case CIL_TYPE_MEMBER:
		type = CIL_KEY_TYPEMEMBER;
		break;
	case CIL_TYPE_CHANGE:
		type = CIL_KEY_TYPECHANGE;
		break;
	default:
		cil_log(CIL_ERR, "Unknown TYPERULE type: %d\n", typerule->rule_kind);
		rc = SEPOL_ERR;
		goto exit;
		break;
	}
	src = typerule->src_str;
	tgt = typerule->tgt_str;
	obj = typerule->obj_str;
	res = typerule->result_str;
	fprintf(cil_out, "(%s %s %s %s %s)\n", type, src, tgt, obj, res);
	rc = SEPOL_OK;
exit:
	return rc;
}

static int cil_write_sens(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_sens *sens = (struct cil_sens *)node->data;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_SENSITIVITY, sens->datum.name);
	return SEPOL_OK;
}

static int cil_write_cat(struct cil_tree_node *node, FILE *cil_out) {
	struct cil_cat *cat = (struct cil_cat *)node->data;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_CATEGORY, cat->datum.name);
	return SEPOL_OK;
}

static int cil_write_senscat(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *sens;
	char *cats = NULL;
	struct cil_senscat *senscat = (struct cil_senscat *)node->data;

	sens = senscat->sens_str;
	rc = cil_unfill_cats(senscat->cats, &cats);
	if (rc != SEPOL_OK)
		goto exit;
	/* TODO: deal with extra/missing parens */
	fprintf(cil_out, "(%s %s (%s))\n", CIL_KEY_SENSCAT, sens, cats);
	rc = SEPOL_OK;
exit:
	free(cats);
	return rc;
}

static int cil_write_catorder(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *ord_str = NULL;
	struct cil_catorder *catord = (struct cil_catorder *)node->data;

	/* cil_unfill_expr() has logic to stringify a cil_list, reuse that. */
	rc = cil_unfill_expr(catord->cat_list_str, &ord_str, 1);
	if (rc != SEPOL_OK)
		goto exit;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_CATORDER, ord_str);
	rc = SEPOL_OK;
exit:
	free(ord_str);
	return rc;
}

static int cil_write_sensorder(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *ord_str = NULL;
	struct cil_sensorder *sensord = (struct cil_sensorder *)node->data;

	/* cil_unfill_expr() has logic to stringify a cil_list, reuse that. */
	rc = cil_unfill_expr(sensord->sens_list_str, &ord_str, 1);
	if (rc != SEPOL_OK)
		goto exit;
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_SENSITIVITYORDER, ord_str);
	rc = SEPOL_OK;
exit:
	free(ord_str);
	return rc;
}

static int cil_write_genfscon(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	char *ctx_str = NULL;

	struct cil_genfscon *genfscon = (struct cil_genfscon *)node->data;
	if (genfscon->context_str != NULL) {
		ctx_str = strdup(genfscon->context_str);
		if (ctx_str == NULL) {
			cil_log(CIL_ERR, "OOM. Unable to copy context string.\n");
            rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		rc = cil_unfill_context(genfscon->context, &ctx_str);
		if (rc != SEPOL_OK)
			goto exit;
	}
	fprintf(cil_out, "(%s %s %s %s)\n", CIL_KEY_GENFSCON, genfscon->fs_str,
            genfscon->path_str, ctx_str);
	rc = SEPOL_OK;
exit:
	free(ctx_str);
	return rc;
}

static int cil_unfill_classperms(struct cil_list_item *curr, char **out_str) {
	int rc = SEPOL_ERR;
	size_t len = 3;
	char *class_str;
	char *perms_str = NULL;
	struct cil_classperms *cp = (struct cil_classperms *)curr->data;

	class_str = cp->class_str;
	len += strlen(class_str) + 1;

	/* fill_perms just calls gen_expr */
	rc = cil_unfill_expr(cp->perm_strs, &perms_str, 1);
	if (rc != SEPOL_OK)
		goto exit;
	len += strlen(perms_str);
	*out_str = cil_malloc(len);
	sprintf(*out_str, "(%s %s)", class_str, perms_str);
	rc = SEPOL_OK;
exit:
	free(perms_str);
	return rc;
}

static int cil_unfill_classperms_list(struct cil_list *classperms, char **out_str, int paren) {
	int rc = SEPOL_ERR;
	struct cil_list_item *curr;
	char *str = NULL;

	/* reuse cil_list to keep track of strings */
	struct cil_list *str_list = NULL;
	cil_list_init(&str_list, CIL_NONE);
	cil_list_for_each(curr, classperms) {
		switch (curr->flavor) {
		case CIL_CLASSPERMS_SET:
			str = strdup(((struct cil_classperms_set *)curr->data)->set_str);
			if (str == NULL) {
				cil_log(CIL_ERR, "OOM. Unable to copy classpermset.\n");
                rc = SEPOL_ERR;
				goto exit;
			}
			break;
		case CIL_CLASSPERMS:
			rc = cil_unfill_classperms(curr, &str);
			if (rc != SEPOL_OK)
				goto exit;
			break;
		default:
			cil_log(CIL_ERR, "Unrecognized classperms flavor\n.");
			goto exit;
		}
		cil_list_append(str_list, CIL_STRING, (void *) str);
		str = NULL;
	}
	rc = __cil_strlist_concat(str_list, out_str, paren);
	if (rc != SEPOL_OK)
		goto exit;
	rc = SEPOL_OK;
exit:
	cil_list_for_each(curr, str_list) {
		free(curr->data);
	}
	cil_list_destroy(&str_list, 0);
	return rc;
}

static int cil_write_fsuse(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	struct cil_fsuse *fsuse = (struct cil_fsuse *)node->data;
	char *type, *fsname;
	char *ctx_str = NULL;

	switch(fsuse->type) {
	case CIL_FSUSE_XATTR:
		type = CIL_KEY_XATTR;
		break;
	case CIL_FSUSE_TASK:
		type = CIL_KEY_TASK;
		break;
	case CIL_FSUSE_TRANS:
		type = CIL_KEY_TRANS;
		break;
	default:
		cil_log(CIL_ERR, "Unrecognized fsuse type\n");
		rc = SEPOL_ERR;
		goto exit;
		break;
	}

	fsname = fsuse->fs_str;
	if (fsuse->context_str != NULL) {
		ctx_str = strdup(fsuse->context_str);
		if (ctx_str == NULL) {
			cil_log(CIL_ERR, "OOM. Unable to copy context string.\n");
			rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		rc = cil_unfill_context(fsuse->context, &ctx_str);
		if (rc != SEPOL_OK)
			goto exit;
	}
	fprintf(cil_out, "(%s %s %s %s)\n", CIL_KEY_FSUSE, type, fsname, ctx_str);
exit:
	free(ctx_str);
	return rc;
}

static int cil_write_constrain(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_ERR;
	struct cil_constrain *cons = (struct cil_constrain *)node->data;
	char *flav;
	char *classperms = NULL;
	char *expr = NULL;

	flav = (node->flavor == CIL_CONSTRAIN) ? CIL_KEY_CONSTRAIN : CIL_KEY_MLSCONSTRAIN;

	rc = cil_unfill_classperms_list(cons->classperms, &classperms, 0);
	if (rc != SEPOL_OK)
		goto exit;

	rc = cil_unfill_expr(cons->str_expr, &expr, 0);
	if (rc != SEPOL_OK)
		goto exit;

	fprintf(cil_out, "(%s %s %s)\n", flav, classperms, expr);
exit:
	free(classperms);
	free(expr);
	return rc;
}

static int cil_write_handleunknown(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_OK;
	struct cil_handleunknown *handunknown = (struct cil_handleunknown *)node->data;
	char *val = NULL;
	switch (handunknown->handle_unknown) {
	case SEPOL_ALLOW_UNKNOWN:
		val = CIL_KEY_HANDLEUNKNOWN_ALLOW;
		break;
	case SEPOL_DENY_UNKNOWN:
		val = CIL_KEY_HANDLEUNKNOWN_DENY;
		break;
	case SEPOL_REJECT_UNKNOWN:
		val = CIL_KEY_HANDLEUNKNOWN_REJECT;
		break;
	default:
		cil_log(CIL_ERR, "Unknown handleunknown value: %d.\n",
			handunknown->handle_unknown);
		rc = SEPOL_ERR;
		goto exit;
		break;
	}
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_HANDLEUNKNOWN, val);
exit:
	return rc;
}

static int cil_write_mls(struct cil_tree_node *node, FILE *cil_out) {
	int rc = SEPOL_OK;
	struct cil_mls *mls = (struct cil_mls *)node->data;
	char *val = NULL;
	switch (mls->value) {
	case CIL_TRUE:
		val = CIL_KEY_CONDTRUE;
		break;
	case CIL_FALSE:
		val = CIL_KEY_CONDFALSE;
		break;
	default:
		cil_log(CIL_ERR, "Unknown mls value: %d.\n", mls->value);
		rc = SEPOL_ERR;
		goto exit;
		break;
	}
	fprintf(cil_out, "(%s %s)\n", CIL_KEY_MLS, val);
exit:
	return rc;
}

static int __cil_write_first_child_helper(struct cil_tree_node *node, void *extra_args)
{
	int rc = SEPOL_ERR;
	struct cil_args_write *args = (struct cil_args_write *) extra_args;
	FILE *cil_out = NULL;

	if (node == NULL || extra_args == NULL) {
		goto exit;
	}

	cil_out = args->cil_out;

	if (node->parent && node->parent->flavor != CIL_ROOT && node->parent->flavor != CIL_SRC_INFO)
		fprintf(cil_out,"(");
	rc = SEPOL_OK;
exit:
	return rc;
}

static int __cil_write_node_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	int rc = SEPOL_OK;
	struct cil_args_write *args = NULL;
	FILE *cil_out = NULL;

	if (node == NULL || extra_args == NULL) {
		goto exit;
	}

	args = extra_args;
	cil_out = args->cil_out;

	switch (node->flavor) {
	case CIL_BLOCK:
		rc = cil_write_unsupported("CIL_BLOCK");
		break;
	case CIL_BLOCKABSTRACT:
		rc = cil_write_unsupported("CIL_BLOCKABSTRACT");
		break;
	case CIL_BLOCKINHERIT:
		rc = cil_write_unsupported("CIL_BLOCKINHERIT");
		break;
	case CIL_IN:
		rc = cil_write_unsupported("CIL_IN");
		break;
	case CIL_POLICYCAP:
		cil_write_policycap(node, cil_out);
		break;
	case CIL_PERM:
		rc = cil_write_perm(node, cil_out);
		break;
	case CIL_MAP_PERM:
		rc = cil_write_unsupported("CIL_MAP_PERM");
		break;
	case CIL_CLASSMAPPING:
		rc = cil_write_unsupported("CIL_CLASSMAPPING");
		break;
	case CIL_CLASS:
		rc = cil_write_class(node, finished, extra_args);
		break;
	case CIL_COMMON:
		rc = cil_write_class(node, finished, extra_args);
		break;
	case CIL_MAP_CLASS:
		rc = cil_write_unsupported("CIL_MAP_CLASS");
		break;
	case CIL_CLASSORDER:
		rc = cil_write_classorder(node, cil_out);
		break;
	case CIL_CLASSPERMISSION:
		rc = cil_write_unsupported("CIL_CLASSPERMISSION");
		break;
	case CIL_CLASSPERMISSIONSET:
		rc = cil_write_unsupported("CIL_CLASSPERMISSIONSET");
		break;
	case CIL_CLASSCOMMON:
		rc = cil_write_classcommon(node, cil_out);
		break;
	case CIL_SID:
		rc = cil_write_sid(node, cil_out);
		break;
	case CIL_SIDCONTEXT:
		rc = cil_write_sidcontext(node, cil_out);
		break;
	case CIL_SIDORDER:
		rc = cil_write_sidorder(node, cil_out);
		break;
	case CIL_USER:
		rc = cil_write_user(node, cil_out);
		break;
	case CIL_USERATTRIBUTE:
		rc = cil_write_unsupported("CIL_USERATTRIBUTE");
		break;
	case CIL_USERATTRIBUTESET:
		rc = cil_write_unsupported("CIL_USERATTRIBUTESET");
		break;
	case CIL_USERROLE:
		rc = cil_write_userrole(node, cil_out);
		break;
	case CIL_USERLEVEL:
		rc = cil_write_userlevel(node, cil_out);
		break;
	case CIL_USERRANGE:
		rc = cil_write_userrange(node, cil_out);
		break;
	case CIL_USERBOUNDS:
		rc = cil_write_unsupported("CIL_USERBOUNDS");
		break;
	case CIL_USERPREFIX:
		rc = cil_write_unsupported("CIL_USERPREFIX");
		break;
	case CIL_ROLE:
		rc = cil_write_role(node, cil_out);
		break;
	case CIL_ROLETYPE:
		rc = cil_write_roletype(node, cil_out);
		break;
	case CIL_ROLEBOUNDS:
		rc = cil_write_unsupported("CIL_ROLEBOUNDS");
		break;
	case CIL_ROLEATTRIBUTE:
		cil_write_roleattribute(node, cil_out);
		break;
	case CIL_ROLEATTRIBUTESET:
		rc = cil_write_unsupported("CIL_ROLEATTRIBUTESET");
		break;
	case CIL_ROLEALLOW:
		rc = cil_write_unsupported("CIL_ROLEALLOW");
		break;
	case CIL_TYPE:
		rc = cil_write_type(node, cil_out);
		break;
	case CIL_TYPEBOUNDS:
		rc = cil_write_unsupported("CIL_TYPEBOUNDS");
		break;
	case CIL_TYPEPERMISSIVE:
		rc = cil_write_typepermissive(node, cil_out);
		break;
	case CIL_TYPEATTRIBUTE:
		rc = cil_write_typeattribute(node, cil_out);
		break;
	case CIL_TYPEATTRIBUTESET:
		rc = cil_write_typeattributeset(node, cil_out);
		break;
    case CIL_EXPANDTYPEATTRIBUTE:
        rc = cil_write_expandtypeattribute(node, cil_out);
        break;
	case CIL_TYPEALIAS:
		rc = cil_write_alias(node, cil_out);
		break;
	case CIL_TYPEALIASACTUAL:
		rc = cil_write_aliasactual(node, cil_out);
		break;
	case CIL_ROLETRANSITION:
		rc = cil_write_unsupported("CIL_ROLETRANSITION");
		break;
	case CIL_NAMETYPETRANSITION:
		rc = cil_write_nametypetransition(node, cil_out);
		break;
	case CIL_RANGETRANSITION:
		rc = cil_write_unsupported("CIL_RANGETRANSITION");
		break;
	case CIL_TUNABLE:
		rc = cil_write_unsupported("CIL_TUNABLE");
		break;
	case CIL_BOOL:
		rc = cil_write_unsupported("CIL_BOOL");
		break;
	case CIL_AVRULE:
	case CIL_AVRULEX:
		rc = cil_write_avrule(node, cil_out);
		break;
	case CIL_PERMISSIONX:
		rc = cil_write_unsupported("CIL_PERMISSIONX");
		break;
	case CIL_TYPE_RULE:
		cil_write_type_rule(node, cil_out);
		break;
	case CIL_SENS:
		rc = cil_write_sens(node, cil_out);
		break;
	case CIL_SENSALIAS:
		rc = cil_write_alias(node, cil_out);
		break;
	case CIL_SENSALIASACTUAL:
		rc = cil_write_aliasactual(node, cil_out);
		break;
	case CIL_CAT:
		rc = cil_write_cat(node, cil_out);
		break;
	case CIL_CATALIAS:
		rc = cil_write_alias(node, cil_out);
		break;
	case CIL_CATALIASACTUAL:
		rc = cil_write_aliasactual(node, cil_out);
		break;
	case CIL_CATSET:
		rc = cil_write_unsupported("CIL_CATSET");
		break;
	case CIL_SENSCAT:
		rc = cil_write_senscat(node, cil_out);
		break;
	case CIL_CATORDER:
		rc = cil_write_catorder(node, cil_out);
		break;
	case CIL_SENSITIVITYORDER:
		rc = cil_write_sensorder(node, cil_out);
		break;
	case CIL_LEVEL:
		rc = cil_write_unsupported("CIL_LEVEL");
		break;
	case CIL_LEVELRANGE:
		rc = cil_write_unsupported("CIL_LEVELRANGE");
		break;
	case CIL_CONTEXT:
		rc = cil_write_unsupported("CIL_CONTEXT");
		break;
	case CIL_NETIFCON:
		rc = cil_write_unsupported("CIL_NETIFCON");
		break;
	case CIL_GENFSCON:
		 rc = cil_write_genfscon(node, cil_out);
		break;
	case CIL_FILECON:
		rc = cil_write_unsupported("CIL_FILECON");
		break;
	case CIL_NODECON:
		rc = cil_write_unsupported("CIL_NODECON");
		break;
	case CIL_PORTCON:
		rc = cil_write_unsupported("CIL_PORTCON");
		break;
	case CIL_PIRQCON:
		rc = cil_write_unsupported("CIL_PIRQCON");
		break;
	case CIL_IOMEMCON:
		rc = cil_write_unsupported("CIL_IOMEMCON");
		break;
	case CIL_IOPORTCON:
		rc = cil_write_unsupported("CIL_IOPORTCON");
		break;
	case CIL_PCIDEVICECON:
		rc = cil_write_unsupported("CIL_PCIDEVICECON");
		break;
	case CIL_DEVICETREECON:
		rc = cil_write_unsupported("CIL_DEVICETREECON");
		break;
	case CIL_FSUSE:
		rc = cil_write_fsuse(node, cil_out);
		break;
	case CIL_CONSTRAIN:
		rc = cil_write_unsupported("CIL_CONSTRAIN");
		break;
	case CIL_MLSCONSTRAIN:
		rc = cil_write_constrain(node, cil_out);
		break;
	case CIL_VALIDATETRANS:
		rc = cil_write_unsupported("CIL_VALIDATETRANS");
		break;
	case CIL_MLSVALIDATETRANS:
		rc = cil_write_unsupported("CIL_MLSVALIDATETRANS");
		break;
	case CIL_CALL:
		rc = cil_write_unsupported("CIL_CALL");
		break;
	case CIL_MACRO:
		rc = cil_write_unsupported("CIL_MACRO");
		break;
	case CIL_NODE:
		rc = cil_write_unsupported("CIL_NODE");
		break;
	case CIL_OPTIONAL:
		rc = cil_write_unsupported("CIL_OPTIONAL");
		break;
	case CIL_IPADDR:
		rc = cil_write_unsupported("CIL_IPADDR");
		break;
	case CIL_CONDBLOCK:
		rc = cil_write_unsupported("CIL_CONDBLOCK");
		break;
	case CIL_BOOLEANIF:
		rc = cil_write_unsupported("CIL_BOOLEANIF");
		break;
	case CIL_TUNABLEIF:
		rc = cil_write_unsupported("CIL_TUNABLEIF");
		break;
	case CIL_DEFAULTUSER:
		rc = cil_write_unsupported("CIL_DEFAULTUSER");
		break;
	case CIL_DEFAULTROLE:
		rc = cil_write_unsupported("CIL_DEFAULTROLE");
		break;
	case CIL_DEFAULTTYPE:
		rc = cil_write_unsupported("CIL_DEFAULTTYPE");
		break;
	case CIL_DEFAULTRANGE:
		rc = cil_write_unsupported("CIL_DEFAULTRANGE");
		break;
	case CIL_SELINUXUSER:
		rc = cil_write_unsupported("CIL_SELINUXUSER");
		break;
	case CIL_SELINUXUSERDEFAULT:
		rc = cil_write_unsupported("CIL_SELINUXUSERDEFAULT");
		break;
	case CIL_HANDLEUNKNOWN:
		rc = cil_write_handleunknown(node, cil_out);
		break;
	case CIL_MLS:
		rc = cil_write_mls(node, cil_out);
		break;
	case CIL_SRC_INFO:
		break;
	case CIL_NONE:
		// TODO: add proper removal support
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	default:
		cil_log(CIL_ERR, "Unknown AST flavor: %d.\n", node->flavor);
		rc = SEPOL_ERR;
		goto exit;
		break;
	}
exit:
	return rc;
}

static int __cil_write_last_child_helper(struct cil_tree_node *node, void *extra_args)
{
	int rc = SEPOL_ERR;
	struct cil_args_write *args = NULL;
	FILE *cil_out = NULL;

	if (node == NULL || extra_args == NULL) {
		goto exit;
	}

	args = extra_args;
	cil_out = args->cil_out;

	if (node->parent && node->parent->flavor != CIL_ROOT && node->parent->flavor != CIL_SRC_INFO) {
		fprintf(cil_out,")");
	}
	rc = SEPOL_OK;
exit:
	return rc;
}

/* main exported function */
int cil_write_ast(struct cil_db *db, const char* path) {
	int rc = SEPOL_ERR;
	struct cil_args_write extra_args;
	FILE *cil_out = NULL;

	cil_out = fopen(path, "we");
	if (cil_out == NULL) {
		cil_log(CIL_ERR, "Failure opening output file for writing AST\n");
		rc = SEPOL_ERR;
		goto exit;
	}

	extra_args.cil_out = cil_out;
	extra_args.db = db;
	rc = cil_tree_walk(db->ast->root, __cil_write_node_helper,
			   __cil_write_first_child_helper,
			   __cil_write_last_child_helper,
			   &extra_args);
	if (rc != SEPOL_OK) {
		cil_log(CIL_INFO, "cil_tree_walk failed, rc: %d\n", rc);
		goto exit;
	}

exit:
	fclose(cil_out);
	cil_out = NULL;
	return rc;
=======
/*
 * Copyright 2011 Tresys Technology, LLC. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY TRESYS TECHNOLOGY, LLC ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL TRESYS TECHNOLOGY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of Tresys Technology, LLC.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>

#include "cil_internal.h"
#include "cil_flavor.h"
#include "cil_list.h"
#include "cil_log.h"
#include "cil_symtab.h"
#include "cil_tree.h"
#include "cil_write_ast.h"


static inline const char *datum_or_str(struct cil_symtab_datum *datum, const char *str)
{
	return datum ? datum->fqn : str;
}

static inline const char *datum_to_str(struct cil_symtab_datum *datum)
{
	return datum ? datum->fqn : "<?DATUM>";
}

static void write_expr(FILE *out, struct cil_list *expr)
{
	struct cil_list_item *curr;
	int notfirst = 0;

	fprintf(out, "(");
	cil_list_for_each(curr, expr) {
		if (notfirst)
			fprintf(out, " ");
		else
			notfirst = 1;
		switch (curr->flavor) {
		case CIL_LIST:
			write_expr(out, curr->data);
			break;
		case CIL_STRING:
			fprintf(out, "%s", (char *)curr->data);
			break;
		case CIL_DATUM:
		case CIL_TYPE:
		case CIL_ROLE:
		case CIL_USER:
		case CIL_SENS:
		case CIL_CAT:
		case CIL_BOOL:
		case CIL_CLASS:
		case CIL_MAP_CLASS:
		case CIL_NAME:
			fprintf(out, "%s", datum_to_str(curr->data));
			break;
		case CIL_OP: {
			const char *op_str;
			enum cil_flavor op_flavor = (enum cil_flavor)(uintptr_t)curr->data;
			switch (op_flavor) {
			case CIL_AND:
				op_str = CIL_KEY_AND;
				break;
			case CIL_OR:
				op_str = CIL_KEY_OR;
				break;
			case CIL_NOT:
				op_str = CIL_KEY_NOT;
				break;
			case CIL_ALL:
				op_str = CIL_KEY_ALL;
				break;
			case CIL_EQ:
				op_str = CIL_KEY_EQ;
				break;
			case CIL_NEQ:
				op_str = CIL_KEY_NEQ;
				break;
			case CIL_XOR:
				op_str = CIL_KEY_XOR;
				break;
			case CIL_RANGE:
				op_str = CIL_KEY_RANGE;
				break;
			case CIL_CONS_DOM:
				op_str = CIL_KEY_CONS_DOM;
				break;
			case CIL_CONS_DOMBY:
				op_str = CIL_KEY_CONS_DOMBY;
				break;
			case CIL_CONS_INCOMP:
				op_str = CIL_KEY_CONS_INCOMP;
				break;
			default:
				op_str = "<?OP>";
				break;
			}
			fprintf(out, "%s", op_str);
			break;
		}
		case CIL_CONS_OPERAND: {
			const char *operand_str;
			enum cil_flavor operand_flavor = (enum cil_flavor)(uintptr_t)curr->data;
			switch (operand_flavor) {
			case CIL_CONS_U1:
				operand_str = CIL_KEY_CONS_U1;
				break;
			case CIL_CONS_U2:
				operand_str = CIL_KEY_CONS_U2;
				break;
			case CIL_CONS_U3:
				operand_str = CIL_KEY_CONS_U3;
				break;
			case CIL_CONS_T1:
				operand_str = CIL_KEY_CONS_T1;
				break;
			case CIL_CONS_T2:
				operand_str = CIL_KEY_CONS_T2;
				break;
			case CIL_CONS_T3:
				operand_str = CIL_KEY_CONS_T3;
				break;
			case CIL_CONS_R1:
				operand_str = CIL_KEY_CONS_R1;
				break;
			case CIL_CONS_R2:
				operand_str = CIL_KEY_CONS_R2;
				break;
			case CIL_CONS_R3:
				operand_str = CIL_KEY_CONS_R3;
				break;
			case CIL_CONS_L1:
				operand_str = CIL_KEY_CONS_L1;
				break;
			case CIL_CONS_L2:
				operand_str = CIL_KEY_CONS_L2;
				break;
			case CIL_CONS_H1:
				operand_str = CIL_KEY_CONS_H1;
				break;
			case CIL_CONS_H2:
				operand_str = CIL_KEY_CONS_H2;
				break;
			default:
				operand_str = "<?OPERAND>";
				break;
			}
			fprintf(out, "%s", operand_str);
			break;
		}
		default:
			fprintf(out, "<?FLAVOR>");
			break;
		}
	}
	fprintf(out, ")");
}

static void write_node_list(FILE *out, struct cil_tree_node *current)
{
	int notfirst = 0;

	fprintf(out, "(");
	while (current) {
		if (notfirst)
			fprintf(out, " ");
		else
			notfirst = 1;

		fprintf(out, "%s", datum_to_str(current->data));
		current = current->next;
	}
	fprintf(out, ")");
}

static void write_string_list(FILE *out, struct cil_list *list)
{
	struct cil_list_item *curr;
	int notfirst = 0;

	if (!list) {
		fprintf(out, "()");
		return;
	}

	fprintf(out, "(");
	cil_list_for_each(curr, list) {
		if (notfirst)
			fprintf(out, " ");
		else
			notfirst = 1;
		fprintf(out, "%s", (char*)curr->data);
	}
	fprintf(out, ")");
}

static void write_datum_list(FILE *out, struct cil_list *list)
{
	struct cil_list_item *curr;
	int notfirst = 0;

	if (!list) {
		fprintf(out, "()");
		return;
	}

	fprintf(out, "(");
	cil_list_for_each(curr, list) {
		if (notfirst)
			fprintf(out, " ");
		else
			notfirst = 1;
		fprintf(out, "%s", datum_to_str(curr->data));
	}
	fprintf(out, ")");
}

static void write_classperms(FILE *out, struct cil_classperms *cp)
{
	if (!cp) {
		fprintf(out, "()");
		return;
	}

	fprintf(out, "(%s ", datum_or_str(DATUM(cp->class), cp->class_str));
	if (cp->perms)
		write_expr(out, cp->perms);
	else
		write_expr(out, cp->perm_strs);
	fprintf(out, ")");
}

static void write_classperms_list(FILE *out, struct cil_list *cp_list)
{
	struct cil_list_item *curr;
	int notfirst = 0;
	int num = 0;

	if (!cp_list) {
		fprintf(out, "()");
		return;
	}

	cil_list_for_each(curr, cp_list) {
		num++;
	}
	if (num > 1)
		fprintf(out, "(");
	cil_list_for_each(curr, cp_list) {
		if (notfirst)
			fprintf(out, " ");
		else
			notfirst = 1;
		if (curr->flavor == CIL_CLASSPERMS) {
			write_classperms(out, curr->data);
		} else {
			struct cil_classperms_set *cp_set = curr->data;
			struct cil_classpermission *cp = cp_set->set;
			if (cp) {
				if (cp->datum.name)
					fprintf(out, "%s", datum_to_str(DATUM(cp)));
				else
					write_classperms_list(out,cp->classperms);
			} else {
				fprintf(out, "%s", cp_set->set_str);
			}
		}
	}
	if (num > 1)
		fprintf(out, ")");
}

static void write_permx(FILE *out, struct cil_permissionx *permx)
{
	if (permx->datum.name) {
		fprintf(out, "%s", datum_to_str(DATUM(permx)));
	} else {
		fprintf(out, "(");
		fprintf(out, "%s ", permx->kind == CIL_PERMX_KIND_IOCTL ? "ioctl" : "<?KIND>");
		fprintf(out, "%s ", datum_or_str(DATUM(permx->obj), permx->obj_str));
		write_expr(out, permx->expr_str);
		fprintf(out, ")");
	}
}

static void write_cats(FILE *out, struct cil_cats *cats)
{
	if (cats->datum_expr) {
		write_expr(out, cats->datum_expr);
	} else {
		write_expr(out, cats->str_expr);
	}
}

static void write_level(FILE *out, struct cil_level *level, int print_name)
{
	if (print_name && level->datum.name) {
		fprintf(out, "%s", datum_to_str(DATUM(level)));
	} else {
		fprintf(out, "(");
		fprintf(out, "%s", datum_or_str(DATUM(level->sens), level->sens_str));
		if (level->cats) {
			fprintf(out, " ");
			write_cats(out, level->cats);
		}
		fprintf(out, ")");
	}
}

static void write_range(FILE *out, struct cil_levelrange *range, int print_name)
{
	if (print_name && range->datum.name) {
		fprintf(out, "%s", datum_to_str(DATUM(range)));
	} else {
		fprintf(out, "(");
		if (range->low)
			write_level(out, range->low, CIL_TRUE);
		else
			fprintf(out, "%s", range->low_str);
		fprintf(out, " ");
		if (range->high)
			write_level(out, range->high, CIL_TRUE);
		else
			fprintf(out, "%s", range->high_str);
		fprintf(out, ")");
	}
}

static void write_context(FILE *out, struct cil_context *context, int print_name)
{
	if (print_name && context->datum.name) {
		fprintf(out, "%s", datum_to_str(DATUM(context)));
	} else {
		fprintf(out, "(");
		fprintf(out, "%s ", datum_or_str(DATUM(context->user), context->user_str));
		fprintf(out, "%s ", datum_or_str(DATUM(context->role), context->role_str));
		fprintf(out, "%s ", datum_or_str(DATUM(context->type), context->type_str));
		if (context->range)
			write_range(out, context->range, CIL_TRUE);
		else
			fprintf(out, "%s", context->range_str);
		fprintf(out, ")");
	}
}

static void write_ipaddr(FILE *out, struct cil_ipaddr *ipaddr)
{
	if (ipaddr->datum.name) {
		fprintf(out, "%s", datum_to_str(DATUM(ipaddr)));
	} else {
		char buf[256];
		if (inet_ntop(ipaddr->family, &ipaddr->ip, buf, 256) == NULL)
			strcpy(buf, "<?IPADDR>");
		fprintf(out, "(%s)", buf);
	}
}

static void write_constrain(FILE *out, struct cil_constrain *cons)
{
	write_classperms_list(out, cons->classperms);
	fprintf(out, " ");
	if (cons->datum_expr)
		write_expr(out, cons->datum_expr);
	else
		write_expr(out, cons->str_expr);
}

static void write_call_args(FILE *out, struct cil_list *args)
{
	struct cil_list_item *item;
	int notfirst = 0;

	fprintf(out, "(");
	cil_list_for_each(item, args) {
		struct cil_args* arg = item->data;
		enum cil_flavor arg_flavor = arg->flavor;
		if (notfirst)
			fprintf(out, " ");
		else
			notfirst = 1;
		switch (arg_flavor) {
		case CIL_TYPE:
		case CIL_ROLE:
		case CIL_USER:
		case CIL_SENS:
		case CIL_CAT:
		case CIL_BOOL:
		case CIL_CLASS:
		case CIL_MAP_CLASS:
		case CIL_NAME: {
			fprintf(out, "%s", datum_or_str(arg->arg, arg->arg_str));
			break;
		}
		case CIL_CATSET: {
			if (arg->arg) {
				struct cil_catset *catset = (struct cil_catset *)arg->arg;
				write_cats(out, catset->cats);
			} else {
				fprintf(out, "%s", arg->arg_str);
			}
			break;
		}
		case CIL_LEVEL: {
			if (arg->arg) {
				struct cil_level *level = (struct cil_level *)arg->arg;
				write_level(out, level, CIL_TRUE);
			} else {
				fprintf(out, "%s", arg->arg_str);
			}
			break;
		}
		case CIL_LEVELRANGE: {
			if (arg->arg) {
				struct cil_levelrange *range = (struct cil_levelrange *)arg->arg;
				write_range(out, range, CIL_TRUE);
			} else {
				fprintf(out, "%s", arg->arg_str);
			}
			break;
		}
		case CIL_IPADDR: {
			if (arg->arg) {
				struct cil_ipaddr *addr = (struct cil_ipaddr *)arg->arg;
				write_ipaddr(out, addr);
			} else {
				fprintf(out, "%s", arg->arg_str);
			}
			break;
		}
		case CIL_CLASSPERMISSION: {
			if (arg->arg) {
				struct cil_classpermission *cp = (struct cil_classpermission *)arg->arg;
				if (cp->datum.name)
					fprintf(out, "%s", datum_to_str(DATUM(cp)));
				else
					write_classperms_list(out, cp->classperms);
			} else {
				fprintf(out, "%s", arg->arg_str);
			}
			break;
		}
		default:
			fprintf(out, "<?ARG:%s>", datum_or_str(arg->arg, arg->arg_str));
			break;
		}
	}
	fprintf(out, ")");
}

static void write_call_args_tree(FILE *out, struct cil_tree_node *arg_node)
{
	while (arg_node) {
		if (arg_node->data) {
			fprintf(out, "%s", (char *)arg_node->data);
		} else if (arg_node->cl_head) {
			fprintf(out, "(");
			write_call_args_tree(out, arg_node->cl_head);
			fprintf(out, ")");
		}
		if (arg_node->next)
			fprintf(out, " ");
		arg_node = arg_node->next;
	}
}

static const char *macro_param_flavor_to_string(enum cil_flavor flavor)
{
	const char *str;
	switch(flavor) {
	case CIL_TYPE:
		str = CIL_KEY_TYPE;
		break;
	case CIL_ROLE:
		str = CIL_KEY_ROLE;
		break;
	case CIL_USER:
		str = CIL_KEY_USER;
		break;
	case CIL_SENS:
		str = CIL_KEY_SENSITIVITY;
		break;
	case CIL_CAT:
		str = CIL_KEY_CATEGORY;
		break;
	case CIL_CATSET:
		str = CIL_KEY_CATSET;
		break;
	case CIL_LEVEL:
		str = CIL_KEY_LEVEL;
		break;
	case CIL_LEVELRANGE:
		str = CIL_KEY_LEVELRANGE;
		break;
	case CIL_CLASS:
		str = CIL_KEY_CLASS;
		break;
	case CIL_IPADDR:
		str = CIL_KEY_IPADDR;
		break;
	case CIL_MAP_CLASS:
		str = CIL_KEY_MAP_CLASS;
		break;
	case CIL_CLASSPERMISSION:
		str = CIL_KEY_CLASSPERMISSION;
		break;
	case CIL_BOOL:
		str = CIL_KEY_BOOL;
		break;
	case CIL_STRING:
		str = CIL_KEY_STRING;
		break;
	case CIL_NAME:
		str = CIL_KEY_NAME;
		break;
	default:
		str = "<?FLAVOR>";
		break;
	}
	return str;
}

void cil_write_src_info_node(FILE *out, struct cil_tree_node *node)
{
	struct cil_src_info *info = node->data;
	if (info->kind == CIL_KEY_SRC_CIL || info->kind == CIL_KEY_SRC_HLL_LMS) {
		fprintf(out, ";;* lms %u %s\n", info->hll_line, info->path);
	} else if (info->kind == CIL_KEY_SRC_HLL_LMX) {
		fprintf(out, ";;* lmx %u %s\n", info->hll_line, info->path);
	} else {
		fprintf(out, ";;* <?SRC_INFO_KIND> %u %s\n", info->hll_line, info->path);
	}
}

void cil_write_ast_node(FILE *out, struct cil_tree_node *node)
{
	if (!node->data) {
		return;
	}

	switch(node->flavor) {
	case CIL_NODE: {
		fprintf(out, "%s\n", (char *)node->data);
		break;
	}
	case CIL_BLOCK: {
		struct cil_block *block = node->data;
		fprintf(out, "(block %s", datum_to_str(DATUM(block)));
		if (!node->cl_head)
			fprintf(out, ")");
		fprintf(out, "\n");
		break;
	}
	case CIL_BLOCKINHERIT: {
		struct cil_blockinherit *inherit = node->data;
		fprintf(out, "(blockinherit %s)\n", datum_or_str(DATUM(inherit->block), inherit->block_str));
		break;
	}
	case CIL_IN: {
		struct cil_in *in = node->data;
		fprintf(out, "(in %s", in->block_str);
		if (!node->cl_head)
			fprintf(out, ")");
		fprintf(out, "\n");
		break;
	}
	case CIL_OPTIONAL: {
		struct cil_optional *optional = node->data;
		fprintf(out, "(optional %s", datum_to_str(DATUM(optional)));
		if (!node->cl_head)
			fprintf(out, ")");
		fprintf(out, "\n");
		break;
	}
	case CIL_BOOLEANIF: {
		struct cil_booleanif *bif = node->data;
		fprintf(out, "(booleanif ");
		if (bif->datum_expr)
			write_expr(out, bif->datum_expr);
		else
			write_expr(out, bif->str_expr);
		if (!node->cl_head)
			fprintf(out, ")");
		fprintf(out, "\n");
		break;
	}
	case CIL_TUNABLEIF: {
		struct cil_tunableif *tif = node->data;
		fprintf(out, "(tunableif ");
		if (tif->datum_expr)
			write_expr(out, tif->datum_expr);
		else
			write_expr(out, tif->str_expr);
		if (!node->cl_head)
			fprintf(out, ")");
		fprintf(out, "\n");
		break;
	}
	case CIL_CONDBLOCK: {
		struct cil_condblock *cb = node->data;
		fprintf(out, "(%s", cb->flavor == CIL_CONDTRUE ? "true" : "false");
		if (!node->cl_head)
			fprintf(out, ")");
		fprintf(out, "\n");
		break;
	}
	case CIL_MACRO: {
		struct cil_macro *macro = node->data;
		struct cil_list_item *curr;
		fprintf(out, "(macro %s (", datum_to_str(DATUM(macro)));
		if (macro->params) {
			cil_list_for_each(curr, macro->params) {
				struct cil_param *param = curr->data;
				fprintf(out, "(%s %s)", macro_param_flavor_to_string(param->flavor), param->str);
			}
		}
		fprintf(out, ")");
		if (!node->cl_head)
			fprintf(out, ")");
		fprintf(out, "\n");
		break;
	}
	case CIL_CALL: {
		struct cil_call *call = node->data;
		fprintf(out, "(call %s", datum_or_str(DATUM(call->macro), call->macro_str));
		if (call->args) {
			fprintf(out, " ");
			write_call_args(out, call->args);
		} else if (call->args_tree) {
			fprintf(out, " ");
			write_call_args_tree(out, call->args_tree->root);
		}
		if (!node->cl_head)
			fprintf(out, ")");
		fprintf(out, "\n");
		break;
	}
	case CIL_BLOCKABSTRACT: {
		struct cil_blockabstract *abstract = node->data;
		fprintf(out, "(blockabstract %s)\n", abstract->block_str);
		break;
	}
	case CIL_MLS: {
		struct cil_mls *mls = node->data;
		fprintf(out, "(mls %s)\n", mls->value ? "true" : "false");
		break;
	}
	case CIL_HANDLEUNKNOWN: {
		struct cil_handleunknown *unknown = node->data;
		fprintf(out, "(handleunknown ");
		if (unknown->handle_unknown == SEPOL_ALLOW_UNKNOWN)
			fprintf(out, "%s", CIL_KEY_HANDLEUNKNOWN_ALLOW);
		else if (unknown->handle_unknown == SEPOL_DENY_UNKNOWN)
			fprintf(out, "%s", CIL_KEY_HANDLEUNKNOWN_DENY);
		else if (unknown->handle_unknown == SEPOL_REJECT_UNKNOWN)
			fprintf(out, "%s", CIL_KEY_HANDLEUNKNOWN_REJECT);
		else
			fprintf(out, "<?UNKNOWN>");
		fprintf(out, ")\n");
		break;
	}
	case CIL_DEFAULTUSER: {
		struct cil_default *def = node->data;
		fprintf(out, "(defaultuser ");
		if (def->class_datums)
			write_datum_list(out, def->class_datums);
		else
			write_string_list(out, def->class_strs);
		if (def->object == CIL_DEFAULT_SOURCE)
			fprintf(out, " source");
		else if (def->object == CIL_DEFAULT_TARGET)
			fprintf(out, " target");
		else
			fprintf(out, " <?DEFAULT>");
		fprintf(out, ")\n");
		break;
	}
	case CIL_DEFAULTROLE: {
		struct cil_default *def = node->data;
		fprintf(out, "(defaultrole ");
		if (def->class_datums)
			write_datum_list(out, def->class_datums);
		else
			write_string_list(out, def->class_strs);
		if (def->object == CIL_DEFAULT_SOURCE)
			fprintf(out, " source");
		else if (def->object == CIL_DEFAULT_TARGET)
			fprintf(out, " target");
		else
			fprintf(out, " <?DEFAULT>");
		fprintf(out, ")\n");
		break;
	}
	case CIL_DEFAULTTYPE: {
		struct cil_default *def = node->data;
		fprintf(out, "(defaulttype ");
		if (def->class_datums)
			write_datum_list(out, def->class_datums);
		else
			write_string_list(out, def->class_strs);
		if (def->object == CIL_DEFAULT_SOURCE)
			fprintf(out, " source");
		else if (def->object == CIL_DEFAULT_TARGET)
			fprintf(out, " target");
		else
			fprintf(out, " <?DEFAULT>");
		fprintf(out, ")\n");
		break;
	}
	case CIL_DEFAULTRANGE: {
		struct cil_defaultrange *def = node->data;
		fprintf(out, "(defaultrange ");
		if (def->class_datums)
			write_datum_list(out, def->class_datums);
		else
			write_string_list(out, def->class_strs);
		if (def->object_range == CIL_DEFAULT_SOURCE_LOW)
			fprintf(out, " source low");
		else if (def->object_range == CIL_DEFAULT_SOURCE_HIGH)
			fprintf(out, " source high");
		else if (def->object_range == CIL_DEFAULT_SOURCE_LOW_HIGH)
			fprintf(out, " source low-high");
		else if (def->object_range == CIL_DEFAULT_TARGET_LOW)
			fprintf(out, " target low");
		else if (def->object_range == CIL_DEFAULT_TARGET_HIGH)
			fprintf(out, " target high");
		else if (def->object_range == CIL_DEFAULT_TARGET_LOW_HIGH)
			fprintf(out, " target low-high");
		else
			fprintf(out, " <?DEFAULT>");
		fprintf(out, ")\n");
		break;
	}
	case CIL_CLASS: {
		struct cil_class *class = node->data;
		fprintf(out, "(class %s ", datum_to_str(DATUM(class)));
		write_node_list(out, node->cl_head);
		fprintf(out, ")\n");
		break;
	}
	case CIL_CLASSORDER: {
		struct cil_classorder *classorder = node->data;
		fprintf(out, "(classorder ");
		write_string_list(out, classorder->class_list_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_COMMON: {
		struct cil_class *common = node->data;
		fprintf(out, "(common %s ", datum_to_str(DATUM(common)));
		write_node_list(out, node->cl_head);
		fprintf(out, ")\n");
		break;
	}
	case CIL_CLASSCOMMON: {
		struct cil_classcommon *cc = node->data;
		fprintf(out, "(classcommon %s %s)\n", cc->class_str, cc->common_str);
		break;
	}
	case CIL_CLASSPERMISSION: {
		struct cil_classpermission *cp = node->data;
		fprintf(out, "(classpermission %s)\n", datum_to_str(DATUM(cp)));
		break;
	}
	case CIL_CLASSPERMISSIONSET: {
		struct cil_classpermissionset *cps = node->data;
		fprintf(out, "(classpermissionset %s ", cps->set_str);
		write_classperms_list(out, cps->classperms);
		fprintf(out, ")\n");
		break;
	}
	case CIL_MAP_CLASS: {
		struct cil_class *map = node->data;
		fprintf(out, "(classmap %s ", datum_to_str(DATUM(map)));
		write_node_list(out, node->cl_head);
		fprintf(out, ")\n");
		break;
	}
	case CIL_CLASSMAPPING: {
		struct cil_classmapping *mapping = node->data;
		fprintf(out, "(classmapping %s %s ", mapping->map_class_str, mapping->map_perm_str);
		write_classperms_list(out, mapping->classperms);
		fprintf(out, ")\n");
		break;
	}
	case CIL_PERMISSIONX: {
		struct cil_permissionx *permx = node->data;
		fprintf(out, "(permissionx %s (", datum_to_str(DATUM(permx)));
		fprintf(out, "%s ", permx->kind == CIL_PERMX_KIND_IOCTL ? "ioctl" : "<?KIND>");
		fprintf(out, "%s ", datum_or_str(DATUM(permx->obj), permx->obj_str));
		write_expr(out, permx->expr_str);
		fprintf(out, "))\n");
		break;
	}
	case CIL_SID: {
		struct cil_sid *sid = node->data;
		fprintf(out, "(sid %s)\n", datum_to_str(DATUM(sid)));
		break;
	}
	case CIL_SIDCONTEXT: {
		struct cil_sidcontext *sidcon = node->data;
		fprintf(out, "(sidcontext %s ", sidcon->sid_str);
		if (sidcon->context)
			write_context(out, sidcon->context, CIL_TRUE);
		else
			fprintf(out, "%s", sidcon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_SIDORDER: {
		struct cil_sidorder *sidorder = node->data;
		fprintf(out, "(sidorder ");
		write_string_list(out, sidorder->sid_list_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_BOOL: {
		struct cil_bool *boolean = node->data;
		fprintf(out, "(boolean %s %s)\n", datum_to_str(DATUM(boolean)), boolean->value ? "true" : "false");
		break;
	}
	case CIL_TUNABLE: {
		struct cil_tunable *tunable = node->data;
		fprintf(out, "(tunable %s %s)\n", datum_to_str(DATUM(tunable)), tunable->value ? "true" : "false");
		break;
	}
	case CIL_SENS: {
		struct cil_sens *sens = node->data;
		fprintf(out, "(sensitivity %s)\n", datum_to_str(DATUM(sens)));
		break;
	}
	case CIL_SENSALIAS: {
		struct cil_alias *alias = node->data;
		fprintf(out, "(sensitivityalias %s)\n", datum_to_str(DATUM(alias)));
		break;
	}
	case CIL_SENSALIASACTUAL: {
		struct cil_aliasactual *aliasactual = node->data;
		fprintf(out, "(sensitivityaliasactual %s %s)\n", aliasactual->alias_str, aliasactual->actual_str);
		break;
	}
	case CIL_CAT: {
		struct cil_cat *cat = node->data;
		fprintf(out, "(category %s)\n", datum_to_str(DATUM(cat)));
		break;
	}
	case CIL_CATALIAS: {
		struct cil_alias *alias = node->data;
		fprintf(out, "(categoryalias %s)\n", datum_to_str(DATUM(alias)));
		break;
	}
	case CIL_CATALIASACTUAL: {
		struct cil_aliasactual *aliasactual = node->data;
		fprintf(out, "(categoryaliasactual %s %s)\n", aliasactual->alias_str, aliasactual->actual_str);
		break;
	}
	case CIL_CATSET: {
		struct cil_catset *catset = node->data;
		fprintf(out, "(categoryset %s ", datum_to_str(DATUM(catset)));
		write_cats(out, catset->cats);
		fprintf(out, ")\n");
		break;
	}
	case CIL_CATORDER: {
		struct cil_catorder *catorder = node->data;
		fprintf(out, "(categoryorder ");
		write_string_list(out, catorder->cat_list_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_SENSCAT: {
		struct cil_senscat *senscat = node->data;
		fprintf(out, "(sensitivitycategory ");
		fprintf(out, "%s ", senscat->sens_str);
		write_cats(out, senscat->cats);
		fprintf(out, ")\n");
		break;
	}
	case CIL_SENSITIVITYORDER: {
		struct cil_sensorder *sensorder = node->data;
		fprintf(out, "(sensitivityorder ");
		write_string_list(out, sensorder->sens_list_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_LEVEL: {
		struct cil_level *level = node->data;
		fprintf(out, "(level %s ", datum_to_str(&level->datum));
		write_level(out, level, CIL_FALSE);
		fprintf(out, ")\n");
		break;
	}
	case CIL_LEVELRANGE: {
		struct cil_levelrange *lvlrange = node->data;
		fprintf(out, "(levelrange %s ", datum_to_str(DATUM(lvlrange)));
		write_range(out, lvlrange, CIL_FALSE);
		fprintf(out, ")\n");
		break;
	}
	case CIL_USER: {
		struct cil_user *user = node->data;
		fprintf(out, "(user %s)\n", datum_to_str(DATUM(user)));
		break;
	}
	case CIL_USERATTRIBUTE: {
		struct cil_userattribute *attr = node->data;
		fprintf(out, "(userattribute %s)\n", datum_to_str(DATUM(attr)));
		break;
	}
	case CIL_USERATTRIBUTESET: {
		struct cil_userattributeset *attr = node->data;
		fprintf(out, "(userattributeset %s ", attr->attr_str);
		if (attr->datum_expr)
			write_expr(out, attr->datum_expr);
		else
			write_expr(out, attr->str_expr);
		fprintf(out, ")\n");
		break;
	}
	case CIL_USERROLE: {
		struct cil_userrole *userrole = node->data;
		fprintf(out, "(userrole ");
		fprintf(out, "%s ", datum_or_str(userrole->user, userrole->user_str));
		fprintf(out, "%s", datum_or_str(userrole->role, userrole->role_str));
		fprintf(out, ")\n");
		break;
	}
	case CIL_USERLEVEL: {
		struct cil_userlevel *userlevel = node->data;
		fprintf(out, "(userlevel %s ", userlevel->user_str);
		if (userlevel->level)
			write_level(out, userlevel->level, CIL_TRUE);
		else
			fprintf(out, "%s", userlevel->level_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_USERRANGE: {
		struct cil_userrange *userrange = node->data;
		fprintf(out, "(userrange %s ", userrange->user_str);
		if (userrange->range)
			write_range(out, userrange->range, CIL_TRUE);
		else
			fprintf(out, "%s", userrange->range_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_USERBOUNDS: {
		struct cil_bounds *bounds = node->data;
		fprintf(out, "(userbounds %s %s)\n", bounds->parent_str, bounds->child_str);
		break;
	}
	case CIL_USERPREFIX: {
		struct cil_userprefix *prefix = node->data;
		fprintf(out, "(userprefix ");
		fprintf(out, "%s ", datum_or_str(DATUM(prefix->user), prefix->user_str));
		fprintf(out, "%s)\n", prefix->prefix_str);
		break;
	}
	case CIL_SELINUXUSER: {
		struct cil_selinuxuser *selinuxuser = node->data;
		fprintf(out, "(selinuxuser %s ", selinuxuser->name_str);
		fprintf(out, "%s ", datum_or_str(DATUM(selinuxuser->user), selinuxuser->user_str));
		if (selinuxuser->range)
			write_range(out, selinuxuser->range, CIL_TRUE);
		else
			fprintf(out, "%s", selinuxuser->range_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_SELINUXUSERDEFAULT: {
		struct cil_selinuxuser *selinuxuser = node->data;
		fprintf(out, "(selinuxuserdefault ");
		fprintf(out, "%s ", datum_or_str(DATUM(selinuxuser->user), selinuxuser->user_str));
		if (selinuxuser->range)
			write_range(out, selinuxuser->range, CIL_TRUE);
		else
			fprintf(out, "%s", selinuxuser->range_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_ROLE: {
		fprintf(out, "(role %s)\n", datum_to_str(node->data));
		break;
	}
	case CIL_ROLEATTRIBUTE: {
		fprintf(out, "(roleattribute %s)\n", datum_to_str(node->data));
		break;
	}
	case CIL_ROLEATTRIBUTESET: {
		struct cil_roleattributeset *attr = node->data;
		fprintf(out, "(roleattributeset %s ", attr->attr_str);
		if (attr->datum_expr)
			write_expr(out, attr->datum_expr);
		else
			write_expr(out, attr->str_expr);
		fprintf(out, ")\n");
		break;
	}
	case CIL_ROLETYPE: {
		struct cil_roletype *roletype = node->data;
		fprintf(out, "(roletype ");
		fprintf(out, "%s ", datum_or_str(DATUM(roletype->role), roletype->role_str));
		fprintf(out, "%s", datum_or_str(DATUM(roletype->type), roletype->type_str));
		fprintf(out, ")\n");
		break;
	}
	case CIL_ROLEBOUNDS: {
		struct cil_bounds *bnds = node->data;
		fprintf(out, "(rolebounds %s %s)\n", bnds->parent_str, bnds->child_str);
		break;
	}
	case CIL_TYPE: {
		fprintf(out, "(type %s)\n", datum_to_str(node->data));
		break;
	}
	case CIL_TYPEALIAS: {
		fprintf(out, "(typealias %s)\n", datum_to_str(node->data));
		break;
	}
	case CIL_TYPEALIASACTUAL: {
		struct cil_aliasactual *aliasactual = node->data;
		fprintf(out, "(typealiasactual %s %s)\n", aliasactual->alias_str, aliasactual->actual_str);
		break;
	}
	case CIL_TYPEATTRIBUTE: {
		fprintf(out, "(typeattribute %s)\n", datum_to_str(node->data));
		break;
	}
	case CIL_TYPEATTRIBUTESET: {
		struct cil_typeattributeset *attr = node->data;
		fprintf(out, "(typeattributeset %s ", attr->attr_str);
		if (attr->datum_expr)
			write_expr(out, attr->datum_expr);
		else
			write_expr(out, attr->str_expr);
		fprintf(out, ")\n");
		break;
	}
	case CIL_EXPANDTYPEATTRIBUTE: {
		struct cil_expandtypeattribute *attr = node->data;
		fprintf(out, "(expandtypeattribute ");
		if (attr->attr_datums)
			write_expr(out, attr->attr_datums);
		else
			write_expr(out, attr->attr_strs);
		fprintf(out, " %s)\n", attr->expand ? "true" : "false");
		break;
	}
	case CIL_TYPEPERMISSIVE: {
		struct cil_typepermissive *tp = node->data;
		fprintf(out, "(typepermissive ");
		fprintf(out, "%s", datum_or_str(DATUM(tp->type), tp->type_str));
		fprintf(out, ")\n");
		break;
	}
	case CIL_TYPEBOUNDS: {
		struct cil_bounds *bounds = node->data;
		fprintf(out, "(typebounds %s %s)\n", bounds->parent_str, bounds->child_str);
		break;
	}
	case CIL_ROLEALLOW: {
		struct cil_roleallow *roleallow = node->data;
		fprintf(out, "(roleallow ");
		fprintf(out, "%s ", datum_or_str(DATUM(roleallow->src), roleallow->src_str));
		fprintf(out, "%s", datum_or_str(DATUM(roleallow->tgt), roleallow->tgt_str));
		fprintf(out, ")\n");
		break;
	}
	case CIL_ROLETRANSITION: {
		struct cil_roletransition *roletrans = node->data;
		fprintf(out, "(roletransition ");
		fprintf(out, "%s ", datum_or_str(DATUM(roletrans->src), roletrans->src_str));
		fprintf(out, "%s ", datum_or_str(DATUM(roletrans->tgt), roletrans->tgt_str));
		fprintf(out, "%s ", datum_or_str(DATUM(roletrans->obj), roletrans->obj_str));
		fprintf(out, "%s", datum_or_str(DATUM(roletrans->result), roletrans->result_str));
		fprintf(out, ")\n");
		break;
	}
	case CIL_AVRULE: {
		struct cil_avrule *rule = node->data;
		if (rule->rule_kind == AVRULE_ALLOWED)
			fprintf(out, "(allow ");
		else if (rule->rule_kind == AVRULE_AUDITALLOW)
			fprintf(out, "(auditallow ");
		else if (rule->rule_kind == AVRULE_DONTAUDIT)
			fprintf(out, "(dontaudit ");
		else if (rule->rule_kind == AVRULE_NEVERALLOW)
			fprintf(out, "(neverallow ");
		else
			fprintf(out, "(<?AVRULE> ");

		fprintf(out, "%s ", datum_or_str(DATUM(rule->src), rule->src_str));
		fprintf(out, "%s ", datum_or_str(DATUM(rule->tgt), rule->tgt_str));
		write_classperms_list(out, rule->perms.classperms);
		fprintf(out, ")\n");
		break;
	}
	case CIL_AVRULEX: {
		struct cil_avrule *rule = node->data;
		if (rule->rule_kind == AVRULE_ALLOWED)
			fprintf(out, "(allowx ");
		else if (rule->rule_kind == AVRULE_AUDITALLOW)
			fprintf(out, "(auditallowx ");
		else if (rule->rule_kind == AVRULE_DONTAUDIT)
			fprintf(out, "(dontauditx ");
		else if (rule->rule_kind == AVRULE_NEVERALLOW)
			fprintf(out, "(neverallowx ");
		else
			fprintf(out, "(<?AVRULEX> ");
		fprintf(out, "%s ", datum_or_str(DATUM(rule->src), rule->src_str));
		fprintf(out, "%s ", datum_or_str(DATUM(rule->tgt), rule->tgt_str));
		if (rule->perms.x.permx_str) {
			fprintf(out, "%s",rule->perms.x.permx_str);
		} else {
			write_permx(out, rule->perms.x.permx);
		}
		fprintf(out, ")\n");
		break;
	}
	case CIL_TYPE_RULE: {
		struct cil_type_rule *rule = node->data;
		if (rule->rule_kind == AVRULE_TRANSITION)
			fprintf(out, "(typetransition ");
		else if (rule->rule_kind == AVRULE_MEMBER)
			fprintf(out, "(typemember ");
		else if (rule->rule_kind == AVRULE_CHANGE)
			fprintf(out, "(typechange ");
		else
			fprintf(out, "(<?TYPERULE> ");
		fprintf(out, "%s ", datum_or_str(DATUM(rule->src), rule->src_str));
		fprintf(out, "%s ", datum_or_str(DATUM(rule->tgt), rule->tgt_str));
		fprintf(out, "%s ", datum_or_str(DATUM(rule->obj), rule->obj_str));
		fprintf(out, "%s", datum_or_str(DATUM(rule->result), rule->result_str));
		fprintf(out, ")\n");
		break;
	}
	case CIL_NAMETYPETRANSITION: {
		struct cil_nametypetransition *rule = node->data;
		fprintf(out, "(typetransition ");
		fprintf(out, "%s ", datum_or_str(DATUM(rule->src), rule->src_str));
		fprintf(out, "%s ", datum_or_str(DATUM(rule->tgt), rule->tgt_str));
		fprintf(out, "%s ", datum_or_str(DATUM(rule->obj), rule->obj_str));
		fprintf(out, "\"%s\" ", datum_or_str(DATUM(rule->name), rule->name_str));
		fprintf(out, "%s", datum_or_str(DATUM(rule->result), rule->result_str));
		fprintf(out, ")\n");
		break;
	}
	case CIL_RANGETRANSITION: {
		struct cil_rangetransition *rule = node->data;
		fprintf(out, "(rangetransition ");
		fprintf(out, "%s ", datum_or_str(DATUM(rule->src), rule->src_str));
		fprintf(out, "%s ", datum_or_str(DATUM(rule->exec), rule->exec_str));
		fprintf(out, "%s ", datum_or_str(DATUM(rule->obj), rule->obj_str));
		if (rule->range)
			write_range(out, rule->range, CIL_TRUE);
		else
			fprintf(out, "%s", rule->range_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_CONSTRAIN: {
		struct cil_constrain *cons = node->data;
		fprintf(out, "(constrain ");
		write_constrain(out, cons);
		fprintf(out, ")\n");
		break;
	}
	case CIL_MLSCONSTRAIN: {
		struct cil_constrain *cons = node->data;
		fprintf(out, "(mlsconstrain ");
		write_constrain(out, cons);
		fprintf(out, ")\n");
		break;
	}
	case CIL_VALIDATETRANS: {
		struct cil_validatetrans *vt = node->data;
		fprintf(out, "(validatetrans ");
		fprintf(out, "%s ", datum_or_str(DATUM(vt->class), vt->class_str));
		if (vt->datum_expr)
			write_expr(out, vt->datum_expr);
		else
			write_expr(out, vt->str_expr);
		fprintf(out, ")\n");
		break;
	}
	case CIL_MLSVALIDATETRANS: {
		struct cil_validatetrans *vt = node->data;
		fprintf(out, "(mlsvalidatetrans ");
		fprintf(out, "%s ", datum_or_str(DATUM(vt->class), vt->class_str));
		if (vt->datum_expr)
			write_expr(out, vt->datum_expr);
		else
			write_expr(out, vt->str_expr);
		fprintf(out, ")\n");
		break;
	}
	case CIL_CONTEXT: {
		struct cil_context *context = node->data;
		fprintf(out, "(context %s ", datum_to_str(DATUM(context)));
		write_context(out, context, CIL_FALSE);
		fprintf(out, ")\n");
		break;
	}
	case CIL_FILECON: {
		struct cil_filecon *filecon = node->data;
		fprintf(out, "(filecon ");
		fprintf(out, "\"%s\" ", filecon->path_str);
		if (filecon->type == CIL_FILECON_FILE)
			fprintf(out, "%s ", CIL_KEY_FILE);
		else if (filecon->type == CIL_FILECON_DIR)
			fprintf(out, "%s ", CIL_KEY_DIR);
		else if (filecon->type == CIL_FILECON_CHAR)
			fprintf(out, "%s ", CIL_KEY_CHAR);
		else if (filecon->type == CIL_FILECON_BLOCK)
			fprintf(out, "%s ", CIL_KEY_BLOCK);
		else if (filecon->type == CIL_FILECON_SOCKET)
			fprintf(out, "%s ", CIL_KEY_SOCKET);
		else if (filecon->type == CIL_FILECON_PIPE)
			fprintf(out, "%s ", CIL_KEY_PIPE);
		else if (filecon->type == CIL_FILECON_SYMLINK)
			fprintf(out, "%s ", CIL_KEY_SYMLINK);
		else if (filecon->type == CIL_FILECON_ANY)
			fprintf(out, "%s ", CIL_KEY_ANY);
		else
			fprintf(out, "<?FILETYPE> ");
		if (filecon->context)
			write_context(out, filecon->context, CIL_TRUE);
		else if (filecon->context_str)
			fprintf(out, "%s", filecon->context_str);
		else
			fprintf(out, "()");
		fprintf(out, ")\n");
		break;
	}
	case CIL_IBPKEYCON: {
		struct cil_ibpkeycon *ibpkeycon = node->data;
		fprintf(out, "(ibpkeycon %s ", ibpkeycon->subnet_prefix_str);
		fprintf(out, "(%d %d) ", ibpkeycon->pkey_low, ibpkeycon->pkey_high);
		if (ibpkeycon->context)
			write_context(out, ibpkeycon->context, CIL_TRUE);
		else if (ibpkeycon->context_str)
			fprintf(out, "%s", ibpkeycon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_PORTCON: {
		struct cil_portcon *portcon = node->data;
		fprintf(out, "(portcon ");
		if (portcon->proto == CIL_PROTOCOL_UDP)
			fprintf(out, " udp ");
		else if (portcon->proto == CIL_PROTOCOL_TCP)
			fprintf(out, " tcp ");
		else if (portcon->proto == CIL_PROTOCOL_DCCP)
			fprintf(out, "dccp ");
		else if (portcon->proto == CIL_PROTOCOL_SCTP)
			fprintf(out, "sctp ");
		else
			fprintf(out, "<?PROTOCOL> ");
		if (portcon->port_low == portcon->port_high)
			fprintf(out, "%d ", portcon->port_low);
		else
			fprintf(out, "(%d %d) ", portcon->port_low, portcon->port_high);
		if (portcon->context)
			write_context(out, portcon->context, CIL_TRUE);
		else
			fprintf(out, "%s", portcon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_NODECON: {
		struct cil_nodecon *nodecon = node->data;
		fprintf(out, "(nodecon ");
		if (nodecon->addr)
			write_ipaddr(out, nodecon->addr);
		else
			fprintf(out, "%s ", nodecon->addr_str);
		fprintf(out, " ");
		if (nodecon->mask)
			write_ipaddr(out, nodecon->mask);
		else
			fprintf(out, "%s ", nodecon->mask_str);
		fprintf(out, " ");
		if (nodecon->context)
			write_context(out, nodecon->context, CIL_TRUE);
		else
			fprintf(out, "%s", nodecon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_GENFSCON: {
		struct cil_genfscon *genfscon = node->data;
		fprintf(out, "(genfscon ");
		fprintf(out, "%s \"%s\" ", genfscon->fs_str, genfscon->path_str);
		if (genfscon->context)
			write_context(out, genfscon->context, CIL_TRUE);
		else
			fprintf(out, "%s", genfscon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_NETIFCON: {
		struct cil_netifcon *netifcon = node->data;
		fprintf(out, "(netifcon %s ", netifcon->interface_str);
		if (netifcon->if_context)
			write_context(out, netifcon->if_context, CIL_TRUE);
		else
			fprintf(out, "%s", netifcon->if_context_str);
		fprintf(out, " ");
		if (netifcon->packet_context)
			write_context(out, netifcon->packet_context, CIL_TRUE);
		else
			fprintf(out, "%s", netifcon->packet_context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_IBENDPORTCON: {
		struct cil_ibendportcon *ibendportcon = node->data;
		fprintf(out, "(ibendportcon %s %u ", ibendportcon->dev_name_str, ibendportcon->port);
		if (ibendportcon->context)
			write_context(out, ibendportcon->context, CIL_TRUE);
		else
			fprintf(out, "%s", ibendportcon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_PIRQCON: {
		struct cil_pirqcon *pirqcon = node->data;
		fprintf(out, "(pirqcon %d ", pirqcon->pirq);
		if (pirqcon->context)
			write_context(out, pirqcon->context, CIL_TRUE);
		else
			fprintf(out, "%s", pirqcon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_IOMEMCON: {
		struct cil_iomemcon *iomemcon = node->data;
		fprintf(out, "(iomemcon (%"PRId64" %"PRId64") ", iomemcon->iomem_low, iomemcon->iomem_high);
		if (iomemcon->context)
			write_context(out, iomemcon->context, CIL_TRUE);
		else
			fprintf(out, "%s", iomemcon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_IOPORTCON: {
		struct cil_ioportcon *ioportcon = node->data;
		fprintf(out, "(ioportcon ");
		if (ioportcon->ioport_low == ioportcon->ioport_high)
			fprintf(out, "%d ", ioportcon->ioport_low);
		else
			fprintf(out, "(%d %d) ", ioportcon->ioport_low, ioportcon->ioport_high);

		if (ioportcon->context)
			write_context(out, ioportcon->context, CIL_TRUE);
		else
			fprintf(out, "%s", ioportcon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_PCIDEVICECON: {
		struct cil_pcidevicecon *pcidevicecon = node->data;
		fprintf(out, "(pcidevicecon %d ", pcidevicecon->dev);
		if (pcidevicecon->context)
			write_context(out, pcidevicecon->context, CIL_TRUE);
		else
			fprintf(out, "%s", pcidevicecon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_DEVICETREECON: {
		struct cil_devicetreecon *devicetreecon = node->data;
		fprintf(out, "(devicetreecon \"%s\" ", devicetreecon->path);
		if (devicetreecon->context)
			write_context(out, devicetreecon->context, CIL_TRUE);
		else
			fprintf(out, "%s", devicetreecon->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_FSUSE: {
		struct cil_fsuse *fsuse = node->data;
		fprintf(out, "(fsuse ");
		if (fsuse->type == CIL_FSUSE_XATTR)
			fprintf(out, "xattr ");
		else if (fsuse->type == CIL_FSUSE_TASK)
			fprintf(out, "task ");
		else if (fsuse->type == CIL_FSUSE_TRANS)
			fprintf(out, "trans ");
		else
			fprintf(out, "<?TYPE> ");
		fprintf(out, "%s ", fsuse->fs_str);
		if (fsuse->context)
			write_context(out, fsuse->context, CIL_TRUE);
		else
			fprintf(out, "%s", fsuse->context_str);
		fprintf(out, ")\n");
		break;
	}
	case CIL_POLICYCAP: {
		struct cil_policycap *polcap = node->data;
		fprintf(out, "(policycap %s)\n", polcap->datum.name);
		break;
	}
	case CIL_IPADDR: {
		struct cil_ipaddr *ipaddr = node->data;
		char buf[256];
		if (inet_ntop(ipaddr->family, &ipaddr->ip, buf, 256) == NULL)
			strcpy(buf, "<?IPADDR>");
		fprintf(out, "(ipaddr %s %s)\n", datum_to_str(&ipaddr->datum), buf);
		break;
	}
	default :
		fprintf(out, "(<?RULE:%s>)\n", cil_node_to_string(node));
		break;
	}
}

/*
 * Tree walk data and helper functions for writing the AST of the various phases
 */

struct cil_write_ast_args {
	FILE *out;
	int depth;
};

/*
 * Helper functions for writing the parse AST
 */

static int __write_parse_ast_node_helper(struct cil_tree_node *node, __attribute__((unused)) uint32_t *finished, void *extra_args)
{
	struct cil_write_ast_args *args = extra_args;

	fprintf(args->out, "%*s", args->depth*4, "");
	if (!node->data) {
		if (node->cl_head)
			fprintf(args->out, "(\n");
		else
			fprintf(args->out, "()\n");
	} else {
		char *str = (char *)node->data;
		size_t len = strlen(str);
		size_t i;

		for (i = 0; i < len; i++) {
			if (isspace(str[i])) {
				fprintf(args->out, "\"%s\"\n", str);
				return SEPOL_OK;
			}
		}

		fprintf(args->out, "%s\n", (char *)node->data);
	}

	return SEPOL_OK;
}

static int __write_parse_ast_first_child_helper(struct cil_tree_node *node, void *extra_args)
{
	struct cil_write_ast_args *args = extra_args;
	struct cil_tree_node *parent = node->parent;

	if (parent->flavor != CIL_ROOT) {
		args->depth++;
	}

	return SEPOL_OK;
}

static int __write_parse_ast_last_child_helper(struct cil_tree_node *node, void *extra_args)
{
	struct cil_write_ast_args *args = extra_args;
	struct cil_tree_node *parent = node->parent;

	if (parent->flavor == CIL_ROOT) {
		return SEPOL_OK;
	}

	args->depth--;
	fprintf(args->out, "%*s", args->depth*4, "");
	fprintf(args->out, ")\n");

	return SEPOL_OK;
}

/*
 * Helper functions for writing the CIL AST for the build and resolve phases
 */

static int __write_cil_ast_node_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	struct cil_write_ast_args *args = extra_args;

	if (node->flavor == CIL_SRC_INFO) {
		cil_write_src_info_node(args->out, node);
		return SEPOL_OK;
	}

	fprintf(args->out, "%*s", args->depth*4, "");

	cil_write_ast_node(args->out, node);

	if (node->flavor == CIL_CLASS || node->flavor == CIL_COMMON || node->flavor == CIL_MAP_CLASS) {
		*finished = CIL_TREE_SKIP_HEAD;
	}

	return SEPOL_OK;
}

static int __write_cil_ast_first_child_helper(struct cil_tree_node *node, void *extra_args)
{
	struct cil_write_ast_args *args = extra_args;
	struct cil_tree_node *parent = node->parent;

	if (parent->flavor != CIL_SRC_INFO && parent->flavor != CIL_ROOT) {
		args->depth++;
	}

	return SEPOL_OK;
}

static int __write_cil_ast_last_child_helper(struct cil_tree_node *node, void *extra_args)
{
	struct cil_write_ast_args *args = extra_args;
	struct cil_tree_node *parent = node->parent;

	if (parent->flavor == CIL_ROOT) {
		return SEPOL_OK;
	} else if (parent->flavor == CIL_SRC_INFO) {
		fprintf(args->out, ";;* lme\n");
		return SEPOL_OK;
	}

	args->depth--;
	fprintf(args->out, "%*s", args->depth*4, "");
	fprintf(args->out, ")\n");

	return SEPOL_OK;
}

int cil_write_ast(FILE *out, enum cil_write_ast_phase phase, struct cil_tree_node *node)
{
	struct cil_write_ast_args extra_args;
	int rc;

	extra_args.out = out;
	extra_args.depth = 0;

	if (phase == CIL_WRITE_AST_PHASE_PARSE) {
		rc = cil_tree_walk(node, __write_parse_ast_node_helper, __write_parse_ast_first_child_helper, __write_parse_ast_last_child_helper, &extra_args);
	} else {
		rc = cil_tree_walk(node, __write_cil_ast_node_helper, __write_cil_ast_first_child_helper, __write_cil_ast_last_child_helper, &extra_args);
	}

	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to write AST\n");
		return SEPOL_ERR;
	}

	return SEPOL_OK;
>>>>>>> BRANCH (38cb18 Update VERSIONs and Python bindings version to 3.3-rc1 for r)
}
