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

#include <cil/cil.h>
#include <cil/android.h>

#include <sepol/policydb.h>
#include <sepol/policydb/conditional.h>

static __attribute__((__noreturn__)) void usage(const char *prog) {
  printf("Usage: %s [OPTION]... FILE...\n", prog);
  printf("\n");
  printf("Options:\n");
  printf("  -b, --base=<file>          (req'd) base policy for versioning.\n");
  printf("  -i, --incremental=<file>   (req'd) CIL incremental policy.\n");
  printf("  -o, --output=<file>        write binary policy to <file>\n");
  printf("  -h, --help                 display usage information\n");
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

/*
 * read_cil_file - Initialize db and parse CIL input file.
 */
static int read_cil_file(struct cil_db **db, char *path) {
  int rc = SEPOL_ERR;
  FILE *file;
  struct stat filedata;
  uint32_t file_size;
  char *buff = NULL;

  cil_db_init(db);
  file = fopen(path, "re");
  if (!file) {
    fprintf(stderr, "Could not open file: %s\n", path);
    goto file_err;
  }
  rc = stat(path, &filedata);
  if (rc == -1) {
    fprintf(stderr, "Could not stat file: %s - %s\n", path, strerror(errno));
    goto err;
  }
  file_size = filedata.st_size;
  buff = malloc(file_size);
  if (buff == NULL) {
    fprintf(stderr, "OOM!\n");
    rc = SEPOL_ERR;
    goto err;
  }
  rc = fread(buff, file_size, 1, file);
  if (rc != 1) {
    fprintf(stderr, "Failure reading file: %s\n", path);
    rc = SEPOL_ERR;
    goto err;
  }
  fclose(file);
  file = NULL;

  /* creates parse_tree */
  rc = cil_add_file(*db, path, buff, file_size);
  if (rc != SEPOL_OK) {
    fprintf(stderr, "Failure adding %s to parse tree\n", path);
    goto parse_err;
  }
  free(buff);

  return SEPOL_OK;
err:
  fclose(file);
parse_err:
  free(buff);
file_err:
  cil_db_destroy(db);
  return rc;
}

int main(int argc, char *argv[]) {
  int rc = SEPOL_ERR;
  sepol_policydb_t *pdb = NULL;
  struct sepol_policy_file *pf_base = NULL;
  FILE *binary_base = NULL;
  char *base = NULL;
  struct cil_db *incremental_db = NULL;
  char *incremental = NULL;
  struct sepol_policy_file *pf_out = NULL;
  FILE *binary_out = NULL;
  char *output = NULL;
  struct stat binarydata;
  uint32_t binary_size;
  int opt_char;
  int opt_index = 0;
  enum cil_log_level log_level = CIL_ERR;
  static struct option long_opts[] = {{"help", no_argument, 0, 'h'},
                                      {"base", required_argument, 0, 'b'},
                                      {"incremental", required_argument, 0, 'i'},
                                      {"verbose", no_argument, 0, 'v'},
                                      {"output", required_argument, 0, 'o'},
                                      {0, 0, 0, 0}};

  while (1) {
    opt_char = getopt_long(argc, argv, "b:i:o:hv", long_opts, &opt_index);
    if (opt_char == -1) {
      break;
    }
    switch (opt_char) {
    case 'v':
      log_level++;
      break;
    case 'b':
      base = strdup(optarg);
      break;
    case 'i':
      incremental = strdup(optarg);
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
  if (base == NULL || output == NULL || incremental == NULL) {
    fprintf(stderr, "Please specify required arguments\n");
    usage(argv[0]);
  }

  cil_set_log_level(log_level);

  /*
   * Read the base binary policy.
   */
  fprintf(stderr, "\nfopen: %s\n", base);
  binary_base = fopen(base, "r");
  if (!binary_base) {
    fprintf(stderr, "Could not open base binary file: %s\n", base);
    rc = SEPOL_ERR;
    goto exit;
  }

  rc = stat(base, &binarydata);
  if (rc == -1) {
    fprintf(stderr, "Could not stat base binary file: %s\n", base);
    rc = SEPOL_ERR;
    goto exit;
  }
  binary_size = binarydata.st_size;
  if (!binary_size) {
    fprintf(stderr, "No binary size.\n");
    binary_base = NULL;
    rc = SEPOL_ERR;
    goto exit;
  }

  rc = sepol_policy_file_create(&pf_base);
  if (rc != 0) {
    fprintf(stderr, "Failed to create policy file: %d\n", rc);
    goto exit;
  }
  sepol_policy_file_set_fp(pf_base, binary_base);

  rc = sepol_policydb_create(&pdb);
  if (rc != 0) {
    fprintf(stderr, "Could not create policy db: %d", rc);
    goto exit;
  }

  rc = sepol_policydb_read(pdb, pf_base);
  if (rc != 0) {
    fprintf(stderr, "Failed to read binary policy: %d\n", rc);
    goto exit;
  }
  policydb_to_stderr(pdb->p);

  /*
   * Add stuff to the policyd db.
   */
  fprintf(stderr, "Reading incremental policy from %s\n.", incremental);
  read_cil_file(&incremental_db, incremental);

  // cil_set_multiple_decls(incremental_db, 1);

  fprintf(stderr, "Compiling incremental policy\n.");
  rc = cil_compile(incremental_db);
  if (rc != SEPOL_OK) {
    fprintf(stderr, "Failed to compile cildb: %d\n", rc);
    goto exit;
  }

  // rc = cil_build_policydb(incremental_db, &pdb);
  // rc = cil_binary_create_allocated_pdb(incremental_db, &pdb);
  rc = cil_amend_policydb(incremental_db, pdb);
  if (rc != SEPOL_OK) {
    fprintf(stderr, "Failed to build policydb\n");
    goto exit;
  }

  fprintf(stderr, "\n\n");
  policydb_to_stderr(pdb->p);

  /*
   * Write the result to file.
   */
  binary_out = fopen(output, "w");
  if (binary_out == NULL) {
    fprintf(stderr, "Failure opening binary %s file for writing\n", output);
    rc = SEPOL_ERR;
    goto exit;
  }

  rc = sepol_policy_file_create(&pf_out);
  if (rc != 0) {
    fprintf(stderr, "Failed to create policy file: %d\n", rc);
    goto exit;
  }
  sepol_policy_file_set_fp(pf_out, binary_out);

  rc = sepol_policydb_write(pdb, pf_out);
  if (rc != 0) {
    fprintf(stderr, "failed to write binary policy: %d\n", rc);
    goto exit;
  }

exit:
  fprintf(stderr, "Secombinerc terminated with: %d\n", rc);
  if (binary_base != NULL) {
    fclose(binary_base);
  }
  if (binary_out != NULL) {
    fclose(binary_out);
  }
  sepol_policydb_free(pdb);
  cil_db_destroy(&incremental_db);
  sepol_policy_file_free(pf_base);
  sepol_policy_file_free(pf_out);
  free(output);
  return rc;
}
