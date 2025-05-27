#include "hashtable.h"
#include <stdio.h>

static element_t *table = NULL;

element_t *table_add(unsigned char hash[SHA_DIGEST_LENGTH], unsigned int ref_count, unsigned int offset) {
	element_t *element;

	HASH_FIND_PTR(table, hash, element);
	if (element != NULL)
		return NULL;

	element = malloc(sizeof *element);
    memcpy(element->hash, hash, SHA_DIGEST_LENGTH);
    element->ref_count = ref_count;
    element->offset = offset;
	HASH_ADD_PTR(table, hash, element);
	return element;
}

element_t *table_find(unsigned char hash[SHA_DIGEST_LENGTH]) {
    element_t *element;
    
	HASH_FIND_PTR(table, hash, element);
    return element;
}

int table_remove(unsigned char hash[SHA_DIGEST_LENGTH]) {
	element_t *element;

	HASH_FIND_PTR(table, hash, element);
	if (element == NULL)
		return 1;

	HASH_DEL(table, element);
	free(element);
	return 0;
}

void table_clear_foreach(void (*func)(element_t *)) {
	element_t *curr, *tmp;

	HASH_ITER(hh, table, curr, tmp) {
		if (func != NULL) func(curr);
		HASH_DEL(table, curr);  /* delete; users advances to next */
		free(curr);             /* optional- if you want to free  */
	}
}

void table_clear() {
	table_clear_foreach(NULL);
}

void table_print(int (*print_func)(const char *format, ...)) {
    element_t *s;

    for (s = table; s != NULL; s = s->hh.next) {
		print_func("hash ");
		for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
			print_func("%02x", s->hash[i]);
		print_func(": off %u, ref_count %u\n", s->hash, s->offset, s->ref_count);
    }
}