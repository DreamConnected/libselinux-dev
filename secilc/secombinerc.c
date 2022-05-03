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

int add_type(policydb_t *pdb) {
  int rc = SEPOL_ERR;
  uint32_t value = 0;
  char *key = NULL;
  type_datum_t *sepol_type = malloc(sizeof(type_datum_t));
  type_datum_init(sepol_type);

  sepol_type->flavor = TYPE_TYPE;

  key = strdup("foo");
  if (key == NULL) {
    goto exit;
  }

  rc = symtab_insert(pdb, SYM_TYPES, key, sepol_type, SCOPE_DECL, 0, &value);
  if (rc != SEPOL_OK) {
    goto exit;
  }
  sepol_type->s.value = value;
  sepol_type->primary = 1;

  return SEPOL_OK;

exit:
  free(key);
  type_datum_destroy(sepol_type);
  free(sepol_type);
  return rc;
}

int main(int argc, char *argv[]) {
  int rc = SEPOL_ERR;
  sepol_policydb_t *pdb = NULL;
  struct sepol_policy_file *pf_in = NULL;
  FILE *binary_in = NULL;
  char *output = NULL;
  struct sepol_policy_file *pf_out = NULL;
  FILE *binary_out = NULL;
  struct stat binarydata;
  uint32_t binary_size;
  int opt_char;
  int opt_index = 0;
  enum cil_log_level log_level = CIL_ERR;
  static struct option long_opts[] = {{"help", no_argument, 0, 'h'},
                                      {"verbose", no_argument, 0, 'v'},
                                      {"output", required_argument, 0, 'o'},
                                      {0, 0, 0, 0}};

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

  fprintf(stderr, "\nfopen: %s\n", argv[optind]);
  binary_in = fopen(argv[optind], "r");
  if (!binary_in) {
    fprintf(stderr, "Could not open binary file: %s\n", argv[optind]);
    rc = SEPOL_ERR;
    goto exit;
  }

  rc = stat(argv[optind], &binarydata);
  if (rc == -1) {
    fprintf(stderr, "Could not stat binary file: %s\n", argv[optind]);
    rc = SEPOL_ERR;
    goto exit;
  }
  binary_size = binarydata.st_size;
  if (!binary_size) {
    fprintf(stderr, "No binary size.\n");
    binary_in = NULL;
    rc = SEPOL_ERR;
    goto exit;
  }

  rc = sepol_policy_file_create(&pf_in);
  if (rc != 0) {
    fprintf(stderr, "Failed to create policy file: %d\n", rc);
    goto exit;
  }
  sepol_policy_file_set_fp(pf_in, binary_in);

  rc = sepol_policydb_create(&pdb);
  if (rc != 0) {
    fprintf(stderr, "Could not create policy db: %d", rc);
    goto exit;
  }

  rc = sepol_policydb_read(pdb, pf_in);
  if (rc != 0) {
    fprintf(stderr, "Failed to read binary policy: %d\n", rc);
    goto exit;
  }

  /*
   * Now add stuff to the policyd db.
   */
  rc = add_type(&(pdb->p));
  if (rc != 0) {
    fprintf(stderr, "Failed to add stuff: %d\n", rc);
    goto exit;
  }

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
  if (binary_in != NULL) {
    fclose(binary_in);
  }
  if (binary_out != NULL) {
    fclose(binary_out);
  }
  sepol_policydb_free(pdb);
  sepol_policy_file_free(pf_in);
  sepol_policy_file_free(pf_out);
  free(output);
  return rc;
}
