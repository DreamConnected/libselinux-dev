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
  printf("  -b, --base=<file>          (req'd) base policy for versioning.\n");
  printf("  -i, --incremental=<file>   (req'd) CIL incremental policy.\n");
  printf("  -o, --output=<file>        write binary policy to <file>\n");
  printf("  -h, --help                 display usage information\n");
  exit(1);
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

  /*
   * Add stuff to the policyd db.
   */
  read_cil_file(&incremental_db, incremental);

  rc = cil_compile(incremental_db);
  if (rc != SEPOL_OK) {
    fprintf(stderr, "Failed to compile cildb: %d\n", rc);
    goto exit;
  }

  rc = cil_build_policydb(incremental_db, &pdb);
  if (rc != SEPOL_OK) {
    fprintf(stderr, "Failed to build policydb\n");
    goto exit;
  }

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
