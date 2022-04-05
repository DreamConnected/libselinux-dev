/*
 * Copyright 2011 Tresys Technology, LLC. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY TRESYS TECHNOLOGY, LLC ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL TRESYS TECHNOLOGY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of Tresys Technology, LLC.
 */

#include <getopt.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef ANDROID
#include <cil/cil.h>
#else
#include <sepol/cil/cil.h>
#endif
#include <sepol/policydb.h>
#include <sepol/policydb/conditional.h>

static __attribute__((__noreturn__)) void usage(const char *prog) {
  printf("Usage: %s [OPTION]... FILE...\n", prog);
  printf("\n");
  printf("Options:\n");
  printf("  -o, --output=<file>            write binary policy to <file>\n");
  printf("                                 (default: policy.<version>)\n");
  printf("  -h, --help                     display usage information\n");
  exit(1);
}

void policydb_to_stderr(const policydb_t pdb) {
  // policy_type.
  fprintf(stderr, "policy_type: %d\n", pdb.policy_type);

  // name
  fprintf(stderr, "name: %s\n", pdb.name);

  // version
  fprintf(stderr, "version: %s\n", pdb.version);

  // unsupported_format, mls
  fprintf(stderr, "target_platform: %d\n", pdb.target_platform);
  fprintf(stderr, "unsupported_format: %d\n", pdb.unsupported_format);
  fprintf(stderr, "mls: %d\n", pdb.mls);

  fprintf(stderr, "symtab:\n");
  fprintf(stderr,
          "\tnprim:\t\t"
          "SYM_COMMONS=%d, SYM_CLASSES=%d, SYM_ROLES=%d, SYM_TYPES=%d, "
          "SYM_USERS=%d, SYM_BOOLS=%d, SYM_LEVELS=%d, SYM_CATS=%d\n",
          pdb.symtab[SYM_COMMONS].nprim, pdb.symtab[SYM_CLASSES].nprim,
          pdb.symtab[SYM_ROLES].nprim, pdb.symtab[SYM_TYPES].nprim,
          pdb.symtab[SYM_USERS].nprim, pdb.symtab[SYM_BOOLS].nprim,
          pdb.symtab[SYM_LEVELS].nprim, pdb.symtab[SYM_CATS].nprim);
  fprintf(stderr,
          "\ttable->size:\t"
          "SYM_COMMONS=%d, SYM_CLASSES=%d, SYM_ROLES=%d, SYM_TYPES=%d, "
          "SYM_USERS=%d, SYM_BOOLS=%d, SYM_LEVELS=%d, SYM_CATS=%d\n",
          pdb.symtab[SYM_COMMONS].table->size,
          pdb.symtab[SYM_CLASSES].table->size,
          pdb.symtab[SYM_ROLES].table->size, pdb.symtab[SYM_TYPES].table->size,
          pdb.symtab[SYM_USERS].table->size, pdb.symtab[SYM_BOOLS].table->size,
          pdb.symtab[SYM_LEVELS].table->size, pdb.symtab[SYM_CATS].table->size);

  // scope.
  fprintf(stderr, "symtab:\n");
  fprintf(stderr,
          "\tnprim:\t\t"
          "SYM_COMMONS=%d, SYM_CLASSES=%d, SYM_ROLES=%d, SYM_TYPES=%d, "
          "SYM_USERS=%d, SYM_BOOLS=%d, SYM_LEVELS=%d, SYM_CATS=%d\n",
          pdb.scope[SYM_COMMONS].nprim, pdb.scope[SYM_CLASSES].nprim,
          pdb.scope[SYM_ROLES].nprim, pdb.scope[SYM_TYPES].nprim,
          pdb.scope[SYM_USERS].nprim, pdb.scope[SYM_BOOLS].nprim,
          pdb.scope[SYM_LEVELS].nprim, pdb.scope[SYM_CATS].nprim);
  fprintf(stderr,
          "\ttable->size:\t"
          "SYM_COMMONS=%d, SYM_CLASSES=%d, SYM_ROLES=%d, SYM_TYPES=%d, "
          "SYM_USERS=%d, SYM_BOOLS=%d, SYM_LEVELS=%d, SYM_CATS=%d\n",
          pdb.scope[SYM_COMMONS].table->size,
          pdb.scope[SYM_CLASSES].table->size, pdb.scope[SYM_ROLES].table->size,
          pdb.scope[SYM_TYPES].table->size, pdb.scope[SYM_USERS].table->size,
          pdb.scope[SYM_BOOLS].table->size, pdb.scope[SYM_LEVELS].table->size,
          pdb.scope[SYM_CATS].table->size);

  // Avrule: global, decl_val_to_struct.
  int c = 0;
  for (avrule_block_t *cur = pdb.global; cur != NULL; cur = cur->next)
    c++;
  fprintf(stderr, "global=%d.\n", c);

  // te_avtab.
  fprintf(stderr, "te_avtab: nel=%d, nslot=%d, mask=%d.\n", pdb.te_avtab.nel,
          pdb.te_avtab.nslot, pdb.te_avtab.mask);

  // te_cond_avtab.
  fprintf(stderr, "te_cond_avtab: nel=%d, nslot=%d, mask=%d.\n",
          pdb.te_cond_avtab.nel, pdb.te_cond_avtab.nslot,
          pdb.te_cond_avtab.mask);

  // cond_list.
  c = 0;
  for (cond_list_t *cur = pdb.cond_list; cur != NULL; cur = cur->next)
    c++;
  fprintf(stderr, "cond_list=%d.\n", c);

  // role_tr.
  c = 0;
  for (role_trans_t *cur = pdb.role_tr; cur != NULL; cur = cur->next)
    c++;
  fprintf(stderr, "role_tr=%d.\n", c);

  // role_allow.
  c = 0;
  for (role_allow_t *cur = pdb.role_allow; cur != NULL; cur = cur->next)
    c++;
  fprintf(stderr, "role_allow=%d.\n", c);

  // ocontexts.
  fprintf(stderr, "ocontexts: ");
  for (int i = 0; i < OCON_NUM; i++) {
    c = 0;
    for (ocontext_t *cur = pdb.ocontexts[i]; cur != NULL; cur = cur->next)
      c++;
    fprintf(stderr, "[%d]=%d%s", i, c, i == OCON_NUM - 1 ? ".\n" : ", ");
  }

  // genfs.
  c = 0;
  for (genfs_t *cur = pdb.genfs; cur != NULL; cur = cur->next)
    c++;
  fprintf(stderr, "genfs=%d.\n", c);

  // range_tr.
  fprintf(stderr, "range_tr: nel=%d, size=%d\n", pdb.range_tr->nel,
          pdb.range_tr->size);

  // filename_trans, filename_trans_count.
  fprintf(stderr, "filename_trans: nel=%d, size=%d\n", pdb.filename_trans->nel,
          pdb.filename_trans->size);

  // type_attr_map.
  fprintf(stderr, "type_attr_map: ");
  // for (int i = 0; i < pdb.p_types.nprim; i++) {
  //   fprintf(stderr, " [%d]=%d", i,
  //           ebitmap_cardinality(&(pdb.type_attr_map[i])));
  // }
  fprintf(stderr, ".\n");

  // attr_type_map.
  fprintf(stderr, "attr_type_map: ");
  // for (int i = 0; i < pdb.p_types.nprim; i++) {
  //   fprintf(stderr, " [%d]=%d", i,
  //           ebitmap_cardinality(&(pdb.attr_type_map[i])));
  // }
  fprintf(stderr, ".\n");

  // policycaps.
  fprintf(stderr, "policycaps: highbit=%d, cardinality=%d\n",
          pdb.policycaps.highbit, ebitmap_cardinality(&(pdb.policycaps)));

  // permissive_map.
  fprintf(stderr, "permissive_map: highbit=%d, cardinality=%d\n",
          pdb.permissive_map.highbit,
          ebitmap_cardinality(&(pdb.permissive_map)));

  // policyvers.
  fprintf(stderr, "policyvers: %d\n", pdb.policyvers);
  // handle_unknown.
  fprintf(stderr, "handle_unknown: %d\n", pdb.handle_unknown);

  // process_class, dir_class.
  fprintf(stderr, "process_class: %d\n", pdb.process_class);
  fprintf(stderr, "dir_class: %d\n", pdb.dir_class);

  // process_trans, process_trans_dyntrans.
  fprintf(stderr, "process_trans: %d\n", pdb.process_trans);
  fprintf(stderr, "process_trans_dyntrans: %d\n", pdb.process_trans_dyntrans);
}

int avrule_concat(policydb_t *dest_db, const policydb_t src_db) {
  avrule_block_t head;
  head.next = dest_db->global;

  // global.
  avrule_block_t *dest_tail = &head;
  int num_dest = 0;
  while (dest_tail->next != NULL) {
    dest_tail = dest_tail->next;
    num_dest++;
  }
  const avrule_block_t *src_cur = src_db.global;
  int num_src = 0;
  while (src_cur != NULL) {
    dest_tail->next = malloc(sizeof(avrule_block_t));
    if (dest_tail->next == NULL) {
      return -1;
    }
    memcpy(dest_tail->next, src_cur, sizeof(avrule_block_t));
    dest_tail = dest_tail->next;
    dest_tail->next = NULL;

    src_cur = src_cur->next;
    num_src++;
  }
  dest_db->global = head.next;

  // decl_val_to_struct.
  dest_db->decl_val_to_struct =
      realloc(dest_db->decl_val_to_struct,
              (num_dest + num_src) * sizeof(avrule_decl_t *));
  if (dest_db->decl_val_to_struct == NULL) {
    return -1;
  }
  memcpy(dest_db->decl_val_to_struct + num_dest, src_db.decl_val_to_struct,
         num_src * sizeof(avrule_decl_t *));

  return num_dest + num_src;
}

int cond_list_concat(policydb_t *dest_db, const policydb_t src_db) {
  cond_node_t dummy_head;
  dummy_head.next = dest_db->cond_list;

  // cond_list.
  cond_node_t *dest_tail = &dummy_head;
  int num_dest = 0;
  while (dest_tail->next != NULL) {
    dest_tail = dest_tail->next;
    num_dest++;
  }
  const cond_node_t *src_cur = src_db.cond_list;
  int num_src = 0;
  while (src_cur != NULL) {
    dest_tail->next = malloc(sizeof(cond_node_t));
    if (dest_tail->next == NULL) {
      return -1;
    }
    memcpy(dest_tail->next, src_cur, sizeof(cond_node_t));
    dest_tail = dest_tail->next;
    dest_tail->next = NULL;

    src_cur = src_cur->next;
    num_src++;
  }

  dest_db->cond_list = dummy_head.next;
  return num_dest + num_src;
}

int role_tr_list_concat(policydb_t *dest_db, const policydb_t src_db) {
  role_trans_t dummy_head;
  dummy_head.next = dest_db->role_tr;

  // role_tr.
  role_trans_t *dest_tail = &dummy_head;
  int num_dest = 0;
  while (dest_tail->next != NULL) {
    dest_tail = dest_tail->next;
    num_dest++;
  }
  const role_trans_t *src_cur = src_db.role_tr;
  int num_src = 0;
  while (src_cur != NULL) {
    dest_tail->next = malloc(sizeof(role_trans_t));
    if (dest_tail->next == NULL) {
      return -1;
    }
    memcpy(dest_tail->next, src_cur, sizeof(role_trans_t));
    dest_tail = dest_tail->next;
    dest_tail->next = NULL;

    src_cur = src_cur->next;
    num_src++;
  }

  dest_db->role_tr = dummy_head.next;
  return num_dest + num_src;
}

int role_allow_list_concat(policydb_t *dest_db, const policydb_t src_db) {
  role_allow_t dummy_head;
  dummy_head.next = dest_db->role_allow;

  // role_allow.
  role_allow_t *dest_tail = &dummy_head;
  int num_dest = 0;
  while (dest_tail->next != NULL) {
    dest_tail = dest_tail->next;
    num_dest++;
  }
  const role_allow_t *src_cur = src_db.role_allow;
  int num_src = 0;
  while (src_cur != NULL) {
    dest_tail->next = malloc(sizeof(role_allow_t));
    if (dest_tail->next == NULL) {
      return -1;
    }
    memcpy(dest_tail->next, src_cur, sizeof(role_allow_t));
    dest_tail = dest_tail->next;
    dest_tail->next = NULL;

    src_cur = src_cur->next;
    num_src++;
  }

  dest_db->role_allow = dummy_head.next;
  return num_dest + num_src;
}

int ocontext_list_concat(policydb_t *dest_db, const policydb_t src_db,
                         int which) {
  ocontext_t dummy_head;
  dummy_head.next = dest_db->ocontexts[which];

  // ocontext[which].
  ocontext_t *dest_tail = &dummy_head;
  int num_dest = 0;
  while (dest_tail->next != NULL) {
    dest_tail = dest_tail->next;
    num_dest++;
  }
  const ocontext_t *src_cur = src_db.ocontexts[which];
  int num_src = 0;
  while (src_cur != NULL) {
    dest_tail->next = malloc(sizeof(ocontext_t));
    if (dest_tail->next == NULL) {
      return -1;
    }
    memcpy(dest_tail->next, src_cur, sizeof(ocontext_t));
    dest_tail = dest_tail->next;
    dest_tail->next = NULL;

    src_cur = src_cur->next;
    num_src++;
  }

  dest_db->ocontexts[which] = dummy_head.next;
  return num_dest + num_src;
}

int genfs_list_concat(policydb_t *dest_db, const policydb_t src_db) {
  genfs_t dummy_head;
  dummy_head.next = dest_db->genfs;

  // genfs.
  genfs_t *dest_tail = &dummy_head;
  int num_dest = 0;
  while (dest_tail->next != NULL) {
    dest_tail = dest_tail->next;
    num_dest++;
  }
  const genfs_t *src_cur = src_db.genfs;
  int num_src = 0;
  while (src_cur != NULL) {
    dest_tail->next = malloc(sizeof(genfs_t));
    if (dest_tail->next == NULL) {
      return -1;
    }
    memcpy(dest_tail->next, src_cur, sizeof(genfs_t));
    dest_tail = dest_tail->next;
    dest_tail->next = NULL;

    src_cur = src_cur->next;
    num_src++;
  }

  dest_db->genfs = dummy_head.next;
  return num_dest + num_src;
}

int merge_hashtable(hashtab_t *dest, const hashtab_t src) {
  for (int i = 0; i < src->size; i++) {
    hashtab_ptr_t cur = src->htable[i];
    while (cur != NULL) {
      int rc = hashtab_insert(*dest, cur->key, cur->datum);
      if (rc != SEPOL_OK && rc != SEPOL_EEXIST) {
        return rc;
      }
      cur = cur->next;
    }
  }
  return SEPOL_OK;
}

int merge_symbols2(policydb_t *dest_db, const policydb_t src_db, int which) {
  int rc = SEPOL_OK;
  // symtab_t *dest_symtab = &(dest_db->symtab[which]);
  uint32_t dest_nprim = dest_db->symtab[which].nprim;
  const symtab_t src_symtab = src_db.symtab[which];
  const uint32_t src_nprim = src_symtab.nprim;

  int c = 0;
  for (int i = 0; i < src_symtab.table->size; i++) {
    hashtab_ptr_t cur = src_symtab.table->htable[i];
    while (cur != NULL) {
      hashtab_key_t key = cur->key;
      hashtab_datum_t datum = cur->datum;
      uint32_t scope =
          (dest_db->policy_type == POLICY_MOD ? SCOPE_REQ : SCOPE_DECL);
      uint32_t avrule_decl_id = c;

      switch (which) {
      case SYM_COMMONS:
        rc = symtab_insert(dest_db, which, key, datum, scope, avrule_decl_id,
                           &(((common_datum_t *)datum)->s.value));
        break;
      case SYM_CLASSES:
        rc = symtab_insert(dest_db, which, key, datum, scope, avrule_decl_id,
                           &(((class_datum_t *)datum)->s.value));
        break;
      case SYM_ROLES:
        rc = symtab_insert(dest_db, which, key, datum, scope, avrule_decl_id,
                           &(((role_datum_t *)datum)->s.value));
        break;
      case SYM_TYPES:
        rc = symtab_insert(dest_db, which, key, datum, scope, avrule_decl_id,
                           &(((common_datum_t *)datum)->s.value));
        break;
      case SYM_USERS:
        rc = symtab_insert(dest_db, which, key, datum, scope, avrule_decl_id,
                           &(((user_datum_t *)datum)->s.value));
        break;
      case SYM_BOOLS:
        rc = symtab_insert(dest_db, which, key, datum, scope, avrule_decl_id,
                           &(((cond_bool_datum_t *)datum)->s.value));
        break;
      case SYM_LEVELS:
        rc = symtab_insert(dest_db, which, key, datum, scope, avrule_decl_id,
                           &(((level_datum_t *)datum)->level->sens));
        break;
      case SYM_CATS:
        rc = symtab_insert(dest_db, which, key, datum, scope, avrule_decl_id,
                           &(((cat_datum_t *)datum)->s.value));
        break;
      default:
        fprintf(stderr, "unexpected sym.\n");
        return SEPOL_ERR;
        break;
      }

      if (rc == 1) {
        fprintf(stderr,
                "success, but symbol already existed as a requirement.\n");
      } else if (rc != 0) {
        fprintf(stderr, "fail for which=%d, i=%d.\n", which, i);
        return SEPOL_ERR;
      }
      c++;
      cur = cur->next;
    }
  }

  // dest_db->sym_val_to_name[which] =
  //     realloc(dest_db->sym_val_to_name[which],
  //             (dest_nprim + src_nprim) * sizeof(char *));
  // if (dest_db->sym_val_to_name[which] == NULL) {
  //   return SEPOL_ERR;
  // }
  // memcpy(dest_db->sym_val_to_name[which] + dest_nprim,
  //        src_db.sym_val_to_name[which], src_nprim * sizeof(char *));

  rc = SEPOL_OK;
  void *dest[] = {NULL, NULL};
  const void *src[] = {NULL, NULL};
  size_t size[] = {0, 0};
  switch (which) {
  case SYM_CLASSES:
    break;
  case SYM_ROLES:
    break;
  case SYM_USERS:
    break;
  case SYM_TYPES:
    // type_attr_map.
    dest_db->type_attr_map = realloc(
        dest_db->type_attr_map, (dest_nprim + src_nprim) * sizeof(ebitmap_t));
    if (dest_db->type_attr_map == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest[0] = dest_db->type_attr_map + dest_nprim;
      src[0] = src_db.type_attr_map;
      size[0] = src_nprim * sizeof(ebitmap_t);
    }
    // attr_type_map.
    dest_db->attr_type_map = realloc(
        dest_db->attr_type_map, (dest_nprim + src_nprim) * sizeof(ebitmap_t));
    if (dest_db->attr_type_map == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest[1] = dest_db->attr_type_map + dest_nprim;
      src[1] = src_db.attr_type_map;
      size[1] = src_nprim * sizeof(ebitmap_t);
    }
    break;
  case SYM_BOOLS:
    break;
  default:
    break;
  }
  if (dest[0] != NULL && src[0] != NULL) {
    memcpy(dest[0], src[0], size[0]);
  }
  if (dest[1] != NULL && src[1] != NULL) {
    memcpy(dest[1], src[1], size[1]);
  }

  return rc;
}

int merge_symbols(policydb_t *dest_db, const policydb_t src_db, int which) {
  int rc = SEPOL_OK;
  symtab_t *dest_symtab = &(dest_db->symtab[which]);
  uint32_t dest_nprim = dest_symtab->nprim;
  const symtab_t src_symtab = src_db.symtab[which];
  const uint32_t src_nprim = src_symtab.nprim;

  if (src_nprim == 0) {
    fprintf(stderr, "no symbols for %d\n", which);
    return SEPOL_OK;
  }

  rc = merge_hashtable(&(dest_symtab->table), src_symtab.table);
  if (rc != SEPOL_OK) {
    fprintf(stderr, "could not merge hashtables: %d\n", rc);
    return rc;
  }
  dest_symtab->nprim += src_symtab.nprim;

  dest_db->sym_val_to_name[which] =
      realloc(dest_db->sym_val_to_name[which],
              (dest_nprim + src_nprim) * sizeof(char *));
  if (dest_db->sym_val_to_name[which] == NULL) {
    return SEPOL_ERR;
  }
  memcpy(dest_db->sym_val_to_name[which] + dest_nprim,
         src_db.sym_val_to_name[which], src_nprim * sizeof(char *));

  rc = SEPOL_OK;
  void *dest[] = {NULL, NULL, NULL};
  const void *src[] = {NULL, NULL, NULL};
  size_t size[] = {0, 0, 0};
  switch (which) {
  case SYM_CLASSES:
    // class_val_to_struct.
    dest_db->class_val_to_struct =
        realloc(dest_db->class_val_to_struct,
                (dest_nprim + src_nprim) * sizeof(class_datum_t *));
    if (dest_db->class_val_to_struct == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest[0] = dest_db->class_val_to_struct + dest_nprim;
      src[0] = src_db.class_val_to_struct;
      size[0] = src_nprim * sizeof(class_datum_t *);
    }
    break;
  case SYM_ROLES:
    // role_val_to_struct.
    dest_db->role_val_to_struct =
        realloc(dest_db->role_val_to_struct,
                (dest_nprim + src_nprim) * sizeof(role_datum_t *));
    if (dest_db->role_val_to_struct == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest[0] = dest_db->role_val_to_struct + dest_nprim;
      src[0] = src_db.role_val_to_struct;
      size[0] = src_nprim * sizeof(role_datum_t *);
    }
    break;
  case SYM_USERS:
    // user_val_to_struct.
    dest_db->user_val_to_struct =
        realloc(dest_db->user_val_to_struct,
                (dest_nprim + src_nprim) * sizeof(user_datum_t *));
    if (dest_db->user_val_to_struct == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest[0] = dest_db->user_val_to_struct + dest_nprim;
      src[0] = src_db.user_val_to_struct;
      size[0] = src_nprim * sizeof(user_datum_t *);
    }
    break;
  case SYM_TYPES:
    // type_val_to_struct.
    dest_db->type_val_to_struct =
        realloc(dest_db->type_val_to_struct,
                (dest_nprim + src_nprim) * sizeof(type_datum_t *));
    if (dest_db->type_val_to_struct == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest[0] = dest_db->type_val_to_struct + dest_nprim;
      src[0] = src_db.type_val_to_struct;
      size[0] = src_nprim * sizeof(type_datum_t *);
    }
    // type_attr_map.
    dest_db->type_attr_map = realloc(
        dest_db->type_attr_map, (dest_nprim + src_nprim) * sizeof(ebitmap_t));
    if (dest_db->type_attr_map == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest[1] = dest_db->type_attr_map + dest_nprim;
      src[1] = src_db.type_attr_map;
      size[1] = src_nprim * sizeof(ebitmap_t);
    }
    // attr_type_map.
    dest_db->attr_type_map = realloc(
        dest_db->attr_type_map, (dest_nprim + src_nprim) * sizeof(ebitmap_t));
    if (dest_db->attr_type_map == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest[2] = dest_db->attr_type_map + dest_nprim;
      src[2] = src_db.attr_type_map;
      size[2] = src_nprim * sizeof(ebitmap_t);
    }
    break;
  case SYM_BOOLS:
    // bool_val_to_struct.
    dest_db->bool_val_to_struct =
        realloc(dest_db->bool_val_to_struct,
                (dest_nprim + src_nprim) * sizeof(cond_bool_datum_t *));
    if (dest_db->bool_val_to_struct == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest[0] = dest_db->bool_val_to_struct + dest_nprim;
      src[0] = src_db.bool_val_to_struct;
      size[0] = src_nprim * sizeof(cond_bool_datum_t *);
    }
    break;
  default:
    break;
  }
  if (dest[0] != NULL && src[0] != NULL) {
    memcpy(dest[0], src[0], size[0]);
  }
  if (dest[1] != NULL && src[1] != NULL) {
    memcpy(dest[1], src[1], size[1]);
  }
  if (dest[2] != NULL && src[2] != NULL) {
    memcpy(dest[2], src[2], size[2]);
  }

  return rc;
}

int merge_policydb(policydb_t *dest, const policydb_t src) {
  int rc;

  // policy_type.
  dest->policy_type = src.policy_type;

  // policyvers.
  dest->policyvers = src.policyvers;

  // name.
  if (src.name != NULL) {
    if (dest->name != NULL) {
      free(dest->name);
    }
    dest->name = strdup(src.name);
  }

  // version
  if (src.version != NULL) {
    if (dest->version != NULL) {
      free(dest->version);
    }
    dest->version = strdup(src.version);
  }

  // unsupported_format, mls
  dest->unsupported_format = src.unsupported_format;
  dest->mls = src.mls;

  // symtab, sym_val_to_name.
#define AD_HOC 0

  if (AD_HOC == 1) {
    for (int i = 0; i < SYM_NUM; i++) {
      rc = merge_symbols(dest, src, i);
      if (rc != SEPOL_OK) {
        fprintf(stderr, "Error merging symbols: %d\n", rc);
        return rc;
      }
    }

    // scope.
    for (int i = 0; i < SYM_NUM; i++) {
      rc = merge_hashtable(&(dest->scope[i].table), src.scope[i].table);
      if (rc != SEPOL_OK) {
        fprintf(stderr, "Error merging scopes: %d\n", rc);
        return rc;
      }
      dest->scope[i].nprim += src.scope[i].nprim;
    }
  } else {
    for (int i = 0; i < SYM_NUM; i++) {
      rc = merge_symbols2(dest, src, i);
      if (rc != SEPOL_OK) {
        fprintf(stderr, "Error merging symbols: %d\n", rc);
        return rc;
      }
    }

    fprintf(stderr, "policydb_index_classes.\n");
    if (policydb_index_classes(dest)) {
      fprintf(stderr, "Error with policydb_index_classes.\n");
      return SEPOL_ERR;
    }

    fprintf(stderr, "policydb_index_others.\n");
    if (policydb_index_others(NULL, dest, 1)) {
      fprintf(stderr, "Error with policydb_index_others.\n");
      return SEPOL_ERR;
    }
    fprintf(stderr, "done.\n");
  }

  // Avrule: global, decl_val_to_struct.
  int c = avrule_concat(dest, src);
  if (c == -1) {
    return SEPOL_ERR;
  }

  // te_avtab.
  if (dest->te_avtab.nel == 0) {
    avtab_alloc(&(dest->te_avtab), src.te_avtab.nel);
  }
  for (int i = 0; i < src.te_avtab.nslot; i++) {
    avtab_ptr_t cur = src.te_avtab.htable[i];
    while (cur != NULL) {
      rc = avtab_insert(&(dest->te_avtab), &(cur->key), &(cur->datum));
      if (rc != SEPOL_OK && rc != SEPOL_EEXIST) {
        fprintf(stderr, "te_avtab insert failed with %d\n", rc);
        return rc;
      }
      cur = cur->next;
    }
  }

  // te_cond_avtab.
  if (dest->te_cond_avtab.nel == 0) {
    avtab_alloc(&(dest->te_cond_avtab), src.te_cond_avtab.nel);
  }
  for (int i = 0; i < src.te_cond_avtab.nslot; i++) {
    avtab_ptr_t cur = src.te_cond_avtab.htable[i];
    while (cur != NULL) {
      rc = avtab_insert(&(dest->te_cond_avtab), &(cur->key), &(cur->datum));
      if (rc != SEPOL_OK && rc != SEPOL_EEXIST) {
        fprintf(stderr, "te_cond_avtab insert failed with %d\n", rc);
        return rc;
      }
      cur = cur->next;
    }
  }

  // cond_list.
  c = cond_list_concat(dest, src);
  if (c == -1) {
    return SEPOL_ERR;
  }

  // role_tr.
  c = role_tr_list_concat(dest, src);
  if (c == -1) {
    return SEPOL_ERR;
  }

  // role_allow.
  c = role_allow_list_concat(dest, src);
  if (c == -1) {
    return SEPOL_ERR;
  }

  // ocontexts.
  for (int i = 0; i < OCON_NUM; i++) {
    c = ocontext_list_concat(dest, src, i);
    if (c == -1) {
      return SEPOL_ERR;
    }
  }

  // genfs.
  c = genfs_list_concat(dest, src);
  if (c == -1) {
    return SEPOL_ERR;
  }

  // range_tr.
  rc = merge_hashtable(&(dest->range_tr), src.range_tr);
  if (rc != SEPOL_OK) {
    fprintf(stderr, "Error merging range_tr: %d\n", rc);
    return rc;
  }

  // filename_trans, filename_trans_count.
  rc = merge_hashtable(&(dest->filename_trans), src.filename_trans);
  if (rc != SEPOL_OK) {
    fprintf(stderr, "Error merging filename_trans: %d\n", rc);
    return rc;
  }
  // dest->filename_trans_count += src.filename_trans_count;
  dest->filename_trans_count = dest->filename_trans->nel;

  // policycaps.
  if (ebitmap_cpy(&(dest->policycaps), &(src.policycaps))) {
    fprintf(stderr, "Error copying policycaps.\n");
    return SEPOL_ERR;
  }

  // permissive_map.
  if (ebitmap_cpy(&(dest->permissive_map), &(src.permissive_map))) {
    fprintf(stderr, "Error copying permissive_map.\n");
    return SEPOL_ERR;
  }

  // handle_unknown.
  dest->handle_unknown = SEPOL_DENY_UNKNOWN; // "deny" should be the default.

  // target_platform, process_class, dir_class.
  // dest->target_platform = SEPOL_TARGET_SELINUX; // "selinux" is the default.
  // dest->process_class = policydb_string_to_security_class(dest, "process");
  // dest->dir_class = policydb_string_to_security_class(dest, "dir");

  // process_trans, process_class, process_trans_dyntrans.
  // dest->process_trans =
  //     policydb_string_to_av_perm(p, p->process_class, "transition");
  // dest->process_trans_dyntrans =
  //     p->process_trans |
  //     policydb_string_to_av_perm(p, p->process_class, "dyntransition");

  return SEPOL_OK;
}

int main(int argc, char *argv[]) {
  int rc = SEPOL_ERR;
  sepol_policydb_t *pdb_merged = NULL;
  struct sepol_policy_file *pf_merged = NULL;
  FILE *binary_merged = NULL;
  char *output_merged = NULL;
  int opt_char;
  int opt_index = 0;
  enum cil_log_level log_level = CIL_ERR;
  static struct option long_opts[] = {{"help", no_argument, 0, 'h'},
                                      {"verbose", no_argument, 0, 'v'},
                                      {"output", required_argument, 0, 'o'},
                                      {0, 0, 0, 0}};
  int i;

  while (1) {
    opt_char = getopt_long(argc, argv, "o:hv", long_opts, &opt_index);
    if (opt_char == -1) {
      break;
    }
    switch (opt_char) {
    case 'v':
      log_level++;
      break;
    case 'o':
      free(output_merged);
      output_merged = strdup(optarg);
      break;
    case 'h':
      usage(argv[0]);
    case '?':
      break;
    default:
      fprintf(stderr, "Unsupported option: %s\n", optarg);
      usage(argv[0]);
    }
  }
  if (optind >= argc) {
    fprintf(stderr, "No files specified\n");
    usage(argv[0]);
  }

  cil_set_log_level(log_level);

  rc = sepol_policydb_create(&pdb_merged);
  if (rc != 0) {
    fprintf(stderr, "Could not create merged policy db: %d", rc);
    goto exit;
  }
  policydb_to_stderr(pdb_merged->p);

  for (i = optind; i < argc; i++) {
    fprintf(stderr, "\nfopen: %s\n", argv[i]);
    sepol_policydb_t *pdb = NULL;
    struct sepol_policy_file *pf = NULL;
    FILE *binary = NULL;
    struct stat binarydata;
    uint32_t binary_size;

    binary = fopen(argv[i], "r");
    if (!binary) {
      fprintf(stderr, "Could not open binary file: %s\n", argv[i]);
      rc = SEPOL_ERR;
      goto exit;
    }

    rc = stat(argv[i], &binarydata);
    if (rc == -1) {
      fprintf(stderr, "Could not stat binary file: %s\n", argv[i]);
      rc = SEPOL_ERR;
      goto exit;
    }
    binary_size = binarydata.st_size;
    if (!binary_size) {
      fprintf(stderr, "No binary size.\n");
      fclose(binary);
      binary = NULL;
      continue;
    }

    rc = sepol_policy_file_create(&pf);
    if (rc != 0) {
      fprintf(stderr, "Failed to create policy file: %d\n", rc);
      goto exit;
    }
    sepol_policy_file_set_fp(pf, binary);

    rc = sepol_policydb_create(&pdb);
    if (rc != 0) {
      fprintf(stderr, "Could not create policy db: %d", rc);
      goto exit;
    }

    rc = sepol_policydb_read(pdb, pf);
    if (rc != 0) {
      fprintf(stderr, "Failed to read binary policy: %d\n", rc);
      goto exit;
    }
    policydb_to_stderr(pdb->p);

    fprintf(stderr, "\n\nmerging binary policy.\n");
    rc = merge_policydb(&(pdb_merged->p), pdb->p);
    if (rc != 0) {
      fprintf(stderr, "Failed to merge binary policies: %d\n", rc);
      goto exit;
    }

    // sepol_policydb_free(pdb);
    free(pdb);
    fclose(binary);
    sepol_policy_file_free(pf);
  }
  fprintf(stderr, "\n\nAD_HOC=%d\n\n", AD_HOC);
  policydb_to_stderr(pdb_merged->p);

  // fprintf(stderr, "Optimizing...");
  // rc = sepol_policydb_optimize(pdb_merged);
  // if (rc != SEPOL_OK) {
  //   fprintf(stderr, "failed to optimize policydb.\n");
  //   goto exit;
  // }
  // fprintf(stderr, "done.\n");

  binary_merged = fopen(output_merged, "w");
  if (binary_merged == NULL) {
    fprintf(stderr, "Failure opening binary %s file for writing\n",
            output_merged);
    rc = SEPOL_ERR;
    goto exit;
  }

  rc = sepol_policy_file_create(&pf_merged);
  if (rc != 0) {
    fprintf(stderr, "Failed to create policy file: %d\n", rc);
    goto exit;
  }

  sepol_policy_file_set_fp(pf_merged, binary_merged);

  fprintf(stderr, "Validating...");
  rc = sepol_policydb_validate(pdb_merged, pf_merged);
  if (rc != 0) {
    fprintf(stderr, "failed to validate merged policy: %d\n", rc);
    goto exit;
  }
  fprintf(stderr, "done.\n");

  fprintf(stderr, "Writing...");
  rc = sepol_policydb_write(pdb_merged, pf_merged);
  if (rc != 0) {
    fprintf(stderr, "failed to write binary policy: %d\n", rc);
    goto exit;
  }
  fprintf(stderr, "done.\n");

  fclose(binary_merged);
  binary_merged = NULL;

exit:
  fprintf(stderr, "Semergerc terminated with: %d\n", rc);
  if (binary_merged != NULL) {
    fclose(binary_merged);
  }
  free(output_merged);
  sepol_policydb_free(pdb_merged);
  sepol_policy_file_free(pf_merged);
  free(output_merged);
  return rc;
}
