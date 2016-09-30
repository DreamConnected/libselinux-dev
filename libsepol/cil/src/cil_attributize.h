#ifndef CIL_ATTRIBUTIZE_H
#define CIL_ATTRIBUTIZE_H
/*
 * code for parsing an AST, grabbing the values that need to be attributized,
 * and replacing them in another AST.  Currently, this just means types and
 * typeattributes.
 */

#include <sepol/policydb/hashtab.h>

#include "cil_build_ast.h"
#include "cil_internal.h"
#include "cil_strpool.h"
#include "cil_symtab.h"
#include "cil_tree.h"

/* take a cil_db which has had the AST built, but not yet resolved */
int cil_extract_attributees(struct cil_db *db, hashtab_t vers_map);
int cil_attributize(struct cil_db *db, hashtab_t vers_map, const char *num);

#endif /* CIL_ATTRIBUTIZE_H */
