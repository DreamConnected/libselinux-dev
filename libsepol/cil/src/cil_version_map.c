#include "cil_version_map.h"

unsigned int ver_map_hash_val(hashtab_t h, const hashtab_key_t key) {
	/* from cil_stpool.c */
	char *p, *keyp;
	size_t size;
	unsigned int val;

	val = 0;
	keyp = (char*)key;
	size = strlen(keyp);
	for (p = keyp; ((size_t) (p - keyp)) < size; p++)
		val =
			(val << 4 | (val >> (8 * sizeof(unsigned int) - 4))) ^ (*p);
	return val & (h->size - 1);
}


int ver_map_key_cmp(hashtab_t h __attribute__ ((unused)),
		    const hashtab_key_t key1, const hashtab_key_t key2) {
	/* hashtab_key_t is just a char* underneath */
	return strcmp((char *)key1, (char *)key2);
}

int ver_map_add(hashtab_t h, const char *key, const char *val) {
	char *tmp_val = hashtab_search(h, (hashtab_key_t) key);
	if (!tmp_val) {
		tmp_val = strdup(val);
		int rc = hashtab_insert(h, (hashtab_key_t)key, tmp_val);
		if (rc != SEPOL_OK)
			return -1;
	}
	return 0;
}

/*
 * version_datum  pointers all refer to memory owned elsewhere, so just free the
 *  datum itself.
 */
static int ver_map_entry_destroy(__attribute__ ((unused))hashtab_key_t k,
				 hashtab_datum_t d, __attribute__ ((unused))void *args) {
	free(d);
	return 0;
}

void ver_map_destroy(hashtab_t h) {
	hashtab_map(h, ver_map_entry_destroy, NULL);
	hashtab_destroy(h);
}
