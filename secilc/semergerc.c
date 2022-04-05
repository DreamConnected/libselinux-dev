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

static __attribute__((__noreturn__)) void usage(const char *prog) {
  printf("Usage: %s [OPTION]... FILE...\n", prog);
  printf("\n");
  printf("Options:\n");
  printf("  -o, --output=<file>            write binary policy to <file>\n");
  printf("                                 (default: policy.<version>)\n");
  printf("  -h, --help                     display usage information\n");
  exit(1);
}

int avrule_concat(policydb_t *dest_db, const policydb_t src_db) {
  avrule_block_t head;
  head.next = dest_db->global;

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
      return SEPOL_ERR;
    }
    memcpy(dest_tail->next, src_cur, 1);
    dest_tail = dest_tail->next;
    dest_tail->next = NULL;

    src_cur = src_cur->next;
    num_src++;
  }

  dest_db->decl_val_to_struct =
      realloc(dest_db->decl_val_to_struct, num_dest + num_src);
  if (dest_db->decl_val_to_struct == NULL) {
    return SEPOL_ERR;
  }
  memcpy(&(dest_db->decl_val_to_struct[num_dest]),
         &(src_db.decl_val_to_struct[0]), num_src);

  dest_db->global = head.next;
  return SEPOL_OK;
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

int merge_symbols(policydb_t *dest_db, const policydb_t src_db, int which) {
  symtab_t *dest_symtab = &(dest_db->symtab[which]);
  uint32_t dest_nprim = dest_symtab->nprim;
  const symtab_t src_symtab = src_db.symtab[which];
  const uint32_t src_nprim = src_symtab.nprim;

  if (src_nprim == 0) {
    // fprintf(stderr, "no symbols for %d\n", which);
    return SEPOL_OK;
  }

  int rc = merge_hashtable(&(dest_symtab->table), src_symtab.table);
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

  for (int i = 0; i < src_nprim; i++) {
    dest_db->sym_val_to_name[which][dest_nprim + i] =
        strdup(src_db.sym_val_to_name[which][i]);
  }

  rc = SEPOL_OK;
  void *dest = NULL, *src = NULL;
  switch (which) {
  case SYM_CLASSES:
    dest_db->class_val_to_struct =
        realloc(dest_db->class_val_to_struct,
                (dest_nprim + src_nprim) * sizeof(class_datum_t *));
    if (dest_db->class_val_to_struct == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest = &(dest_db->class_val_to_struct[dest_nprim]);
      src = &(src_db.class_val_to_struct[0]);
    }
    break;
  case SYM_ROLES:
    dest_db->role_val_to_struct =
        realloc(dest_db->role_val_to_struct,
                (dest_nprim + src_nprim) * sizeof(role_datum_t *));
    if (dest_db->role_val_to_struct == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest = &(dest_db->role_val_to_struct[dest_nprim]);
      src = &(src_db.role_val_to_struct[0]);
    }
    break;
  case SYM_USERS:
    dest_db->user_val_to_struct =
        realloc(dest_db->user_val_to_struct,
                (dest_nprim + src_nprim) * sizeof(user_datum_t *));
    if (dest_db->user_val_to_struct == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest = &(dest_db->user_val_to_struct[dest_nprim]);
      src = &(src_db.user_val_to_struct[0]);
    }
    break;
  case SYM_TYPES:
    dest_db->type_val_to_struct =
        realloc(dest_db->type_val_to_struct,
                (dest_nprim + src_nprim) * sizeof(type_datum_t *));
    if (dest_db->type_val_to_struct == NULL) {
      rc = SEPOL_ERR;
    } else {
      dest = &(dest_db->type_val_to_struct[dest_nprim]);
      src = &(src_db.type_val_to_struct[0]);
    }
    break;
  default:
    break;
  }
  if (src != NULL && dest != NULL) {
    memcpy(src, dest, src_nprim);
  }

  return rc;
}

int merge_policydb(policydb_t *dest, const policydb_t src) {
  int rc;

  // policyvers.
  if (dest->policyvers == 0) {
    dest->policyvers = src.policyvers;
  } else if (dest->policyvers != src.policyvers) {
    fprintf(stderr, "Policy version mismatch: %d vs %d\n", dest->policyvers,
            src.policyvers);
    return SEPOL_ERR;
  }
  fprintf(stderr, "policyvers: %d\n", dest->policyvers);

  // name
  if (src.name != NULL) {
    if (dest->name != NULL) {
      free(dest->name);
    }
    dest->name = strdup(src.name);
  }
  fprintf(stderr, "merged sepolicy name: %s\n", dest->name);

  // version
  if (src.version != NULL) {
    if (dest->version != NULL) {
      free(dest->version);
    }
    dest->version = strdup(src.version);
  }
  fprintf(stderr, "merged sepolicy version: %s\n", dest->version);

  // unsupported_format, mls
  dest->unsupported_format = src.unsupported_format;
  fprintf(stderr, "unsupported_format: %d\n", dest->policyvers);
  dest->mls = src.mls;
  fprintf(stderr, "mls: %d\n", dest->mls);

  for (int i = 0; i < SYM_NUM; i++) {
    // symtab, sym_val_to_name.
    rc = merge_symbols(dest, src, i);
    if (rc != SEPOL_OK) {
      fprintf(stderr, "Error merging symbols: %d\n", rc);
      return rc;
    }

    // scope
    rc = merge_hashtable(&(dest->scope[i].table), src.scope[i].table);
    if (rc != SEPOL_OK) {
      fprintf(stderr, "Error merging scopes: %d\n", rc);
      return rc;
    }
    dest->scope[i].nprim += src.scope[i].nprim;
  }
  fprintf(stderr,
          "symtab.nprim: "
          "SYM_COMMONS=%d, SYM_CLASSES=%d, SYM_ROLES=%d, SYM_TYPES=%d, "
          "SYM_USERS=%d, SYM_BOOLS=%d, SYM_LEVELS=%d, SYM_CATS=%d\n",
          dest->symtab[SYM_COMMONS].nprim, dest->symtab[SYM_CLASSES].nprim,
          dest->symtab[SYM_ROLES].nprim, dest->symtab[SYM_TYPES].nprim,
          dest->symtab[SYM_USERS].nprim, dest->symtab[SYM_BOOLS].nprim,
          dest->symtab[SYM_LEVELS].nprim, dest->symtab[SYM_CATS].nprim);
  fprintf(stderr,
          "scope.nprim: "
          "SYM_COMMONS=%d, SYM_CLASSES=%d, SYM_ROLES=%d, SYM_TYPES=%d, "
          "SYM_USERS=%d, SYM_BOOLS=%d, SYM_LEVELS=%d, SYM_CATS=%d\n",
          dest->scope[SYM_COMMONS].nprim, dest->scope[SYM_CLASSES].nprim,
          dest->scope[SYM_ROLES].nprim, dest->scope[SYM_TYPES].nprim,
          dest->scope[SYM_USERS].nprim, dest->scope[SYM_BOOLS].nprim,
          dest->scope[SYM_LEVELS].nprim, dest->scope[SYM_CATS].nprim);

  // Avrule: global, decl_val_to_struct.
  rc = avrule_concat(dest, src);
  if (rc != SEPOL_OK) {
    fprintf(stderr, "Error with avrule concat: %d\n", rc);
  }

  // te_avtab.
  int c = 0;
  for (int i = 0; i < src.te_avtab.nslot; i++) {
    avtab_ptr_t cur = src.te_avtab.htable[i];
    while (cur != NULL) {
      avtab_insert(&(dest->te_avtab), &(cur->key), &(cur->datum));
      cur = cur->next;
      c++;
    }
  }
  fprintf(stderr, "Inserted %d avtab tables.\n", c);

  return SEPOL_OK;
}

int main(int argc, char *argv[]) {
  int rc = SEPOL_ERR;
  sepol_policydb_t *pdb = NULL;
  sepol_policydb_t *pdb_merged = NULL;
  struct sepol_policy_file *pf = NULL;
  struct sepol_policy_file *pf_output = NULL;
  FILE *binary = NULL;
  struct stat binarydata;
  uint32_t binary_size;
  char *buffer = NULL;
  char *output = NULL;
  int opt_char;
  int opt_index = 0;
  char *fc_buf = NULL;
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
      free(output);
      output = strdup(optarg);
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

  for (i = optind; i < argc; i++) {
    fprintf(stderr, "\nfopen: %s\n", argv[i]);

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

    rc = merge_policydb(&(pdb_merged->p), pdb->p);
    if (rc != 0) {
      fprintf(stderr, "Failed to merge binary policies: %d\n", rc);
      goto exit;
    }

    sepol_policydb_free(pdb);
    fclose(binary);
    pdb = NULL;
    binary = NULL;
  }

  // rc = sepol_policydb_validate(pdb_merged, pf);
  // if (rc != 0) {
  // 	fprintf(stderr, "Validation failed on merged policy: %d\n", rc);
  // 	goto exit;
  // }

  binary = fopen(output, "w");
  if (binary == NULL) {
    fprintf(stderr, "Failure opening binary %s file for writing\n", output);
    rc = SEPOL_ERR;
    goto exit;
  }

  rc = sepol_policy_file_create(&pf_output);
  if (rc != 0) {
    fprintf(stderr, "Failed to create policy file: %d\n", rc);
    goto exit;
  }

  sepol_policy_file_set_fp(pf_output, binary);

  rc = sepol_policydb_write(pdb, pf_output);
  if (rc != 0) {
    fprintf(stderr, "Failed to write binary policy: %d\n", rc);
    goto exit;
  }

  fclose(binary);
  binary = NULL;

exit:
  fprintf(stderr, "Semergerc terminated with: %d\n", rc);
  if (binary != NULL) {
    fclose(binary);
  }
  free(buffer);
  free(output);
  sepol_policydb_free(pdb);
  sepol_policydb_free(pdb_merged);
  sepol_policy_file_free(pf);
  sepol_policy_file_free(pf_output);
  free(fc_buf);
  return rc;
}
