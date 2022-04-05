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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>

#ifdef ANDROID
#include <cil/cil.h>
#else
#include <sepol/cil/cil.h>
#endif
#include <sepol/policydb.h>

static __attribute__((__noreturn__)) void usage(const char *prog)
{
	printf("Usage: %s [OPTION]... FILE...\n", prog);
	printf("\n");
	printf("Options:\n");
	printf("  -o, --output=<file>            write binary policy to <file>\n");
	printf("                                 (default: policy.<version>)\n");
	printf("  -h, --help                     display usage information\n");
	exit(1);
}

int merge_hashtable(hashtab_t *sink, const hashtab_t source) {
  for (int i = 0; i < source->size; i++) {
    hashtab_ptr_t cur = source->htable[i];
    while (cur != NULL) {
      int rc = hashtab_insert(*sink, cur->key, cur->datum);
      if (rc != SEPOL_OK) {
        return rc;
      }
      cur = cur->next;
    }
  }
  return SEPOL_OK;
}

int merge_symbols(policydb_t *sink_db, const policydb_t source_db, int which) {
  symtab_t *sink_symtab = &(sink_db->symtab[which]);
  uint32_t sink_nprim = sink_symtab->nprim;
  const symtab_t source_symtab = source_db.symtab[which];
  const uint32_t source_nprim = source_symtab.nprim;

//   if(source_nprim == 0) {
//     fprintf(stderr, "no symbols for %d\n", which);
//     return SEPOL_OK;
//   }

  int rc = merge_hashtable(&(sink_symtab->table), source_symtab.table);
  if (rc != SEPOL_OK) {
    return rc;
  }
  sink_symtab->nprim += source_symtab.nprim;

  sink_db->sym_val_to_name[which] =
      realloc(sink_db->sym_val_to_name[which],
              (sink_nprim + source_nprim) * sizeof(char *));
  if (sink_db->sym_val_to_name[which] == NULL) {
    return SEPOL_ERR;
  }

  for (int i = 0; i < source_nprim; i++) {
    sink_db->sym_val_to_name[which][sink_nprim + i] = strdup(source_db.sym_val_to_name[which][i]);
  }

  rc = merge_hashtable(&(sink_db->scope[which].table), source_db.scope[which].table);
  if (rc != SEPOL_OK) {
    return rc;
  }
  sink_db->scope[which].nprim += source_db.scope[which].nprim;

  return SEPOL_OK;
}

int merge_policydb(policydb_t *sink, const policydb_t source) {
  if (sink->policyvers == 0) {
    sink->policyvers = source.policyvers;
  } else if (sink->policyvers != source.policyvers) {
    fprintf(stderr, "Policy version mismatch: %d vs %d\n", sink->policyvers,
            source.policyvers);
    return SEPOL_ERR;
  }
  fprintf(stderr, "policyvers: %d\n", sink->policyvers);

  if (source.name != NULL) {
    if (sink->name != NULL) {
      free(sink->name);
    }
    sink->name = strdup(source.name);
  }
  fprintf(stderr, "merged sepolicy name: %s\n", sink->name);

  if (source.version != NULL) {
    if (sink->version != NULL) {
      free(sink->version);
    }
    sink->version = strdup(source.version);
  }
  fprintf(stderr, "merged sepolicy version: %s\n", sink->version);

  sink->unsupported_format = source.unsupported_format;
  fprintf(stderr, "unsupported_format: %d\n", sink->policyvers);
  sink->mls = source.mls;
  fprintf(stderr, "mls: %d\n", sink->mls);

  // TODO(): do not copy, these are numbers that should sum up
  // memccpy(merged->symtab, in.symtab, sizeof(in.symtab), SYM_NUM);

  for (int i = 0; i < SYM_NUM; i++) {
    // merge_symtab(&(sink->symtab[i]), source.symtab[i]);
    merge_symbols(sink, source, i);
  }
  fprintf(stderr, "symtab.nprim: "
          "SYM_COMMONS=%d, SYM_CLASSES=%d, SYM_ROLES=%d, SYM_TYPES=%d, "
          "SYM_USERS=%d, SYM_BOOLS=%d, SYM_LEVELS=%d, SYM_CATS=%d\n",
          sink->symtab[SYM_COMMONS].nprim, sink->symtab[SYM_CLASSES].nprim,
          sink->symtab[SYM_ROLES].nprim, sink->symtab[SYM_TYPES].nprim,
          sink->symtab[SYM_USERS].nprim, sink->symtab[SYM_BOOLS].nprim,
          sink->symtab[SYM_LEVELS].nprim, sink->symtab[SYM_CATS].nprim);
  fprintf(stderr, "scope.nprim: "
          "SYM_COMMONS=%d, SYM_CLASSES=%d, SYM_ROLES=%d, SYM_TYPES=%d, "
          "SYM_USERS=%d, SYM_BOOLS=%d, SYM_LEVELS=%d, SYM_CATS=%d\n",
          sink->scope[SYM_COMMONS].nprim, sink->scope[SYM_CLASSES].nprim,
          sink->scope[SYM_ROLES].nprim, sink->scope[SYM_TYPES].nprim,
          sink->scope[SYM_USERS].nprim, sink->scope[SYM_BOOLS].nprim,
          sink->scope[SYM_LEVELS].nprim, sink->scope[SYM_CATS].nprim);

  return SEPOL_OK;
}

int main(int argc, char *argv[])
{
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
	static struct option long_opts[] = {
		{"help", no_argument, 0, 'h'},
		{"verbose", no_argument, 0, 'v'},
		{"output", required_argument, 0, 'o'},
		{0, 0, 0, 0}
	};
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
