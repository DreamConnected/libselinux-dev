#ifndef CIL_VERS_MAP_H
#define CIL_VERS_MAP_H

#include <stdlib.h>
#include <sepol/policydb/hashtab.h>
#include <string.h>

#include "cil_tree.h"

#define VER_MAP_SZ (1 << 12)

/* added to hashmap */
struct version_datum {
    struct cil_db *db;
    struct cil_tree_node *ast_node;
    char *orig_name;
};

int ver_map_add(hashtab_t h, const char *key, const char *val);
unsigned int ver_map_hash_val(hashtab_t h, const hashtab_key_t key);
int ver_map_key_cmp(hashtab_t h, const hashtab_key_t key1, const hashtab_key_t key2);
void ver_map_destroy(hashtab_t h);

#endif /* CIL_VERS_MAP_H */
