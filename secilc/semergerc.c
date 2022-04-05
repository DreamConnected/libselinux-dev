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

#define ATRACE_TAG ATRACE_TAG_ALWAYS
#include <cutils/trace.h>

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

int main(int argc, char *argv[])
{
	int rc = SEPOL_ERR;
	sepol_policydb_t *pdb = NULL;
	struct sepol_policy_file *pf = NULL;
	FILE *binary = NULL;
	struct stat binarydata;
	uint32_t binary_size;
	// FILE *file = NULL;
	// struct stat filedata;
	// uint32_t file_size;
	char *buffer = NULL;
	char *output = NULL;
	struct cil_db *db = NULL;
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

	ATRACE_BEGIN("fopen-fread");
	for (i = optind; i < argc; i++) {
        fprintf(stderr, "fopen: %s\n", argv[i]);

		binary = fopen(argv[i], "r");
		if (!binary) {
			fprintf(stderr, "Could not open binary file: %s\n", argv[i]);
			rc = SEPOL_ERR;
			goto exit;
		}

        fprintf(stderr, "stat\n");
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

        fprintf(stderr, "sepol_policy_file_create\n");
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

        fprintf(stderr, "sepol_policydb_read\n");
        rc = sepol_policydb_read(pdb, pf);
        if (rc != 0) {
            fprintf(stderr, "Failed to read binary policy: %d\n", rc);
            goto exit;
        }

        fclose(binary);
        binary = NULL;

        fprintf(stderr, "endloop\n");
	}
	ATRACE_END();

	rc = SEPOL_OK;
    fprintf(stderr, "Binary policy read successfully: %d\n", rc);

exit:
	if (binary != NULL) {
		fclose(binary);
	}
	free(buffer);
	free(output);
	cil_db_destroy(&db);
	sepol_policydb_free(pdb);
	sepol_policy_file_free(pf);
	free(fc_buf);
	return rc;
}
