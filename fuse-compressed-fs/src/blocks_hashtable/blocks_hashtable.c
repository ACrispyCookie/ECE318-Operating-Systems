#include "blocks_hashtable.h"
#include "log.h"
#include <stdio.h>

blocks_hash_element_t *blocks_table_add(blocks_hash_element_t **table, const unsigned char hash[SHA_DIGEST_LENGTH], unsigned int ref_count, unsigned int block_index) {
	blocks_hash_element_t *element;

	HASH_FIND_PTR(*table, hash, element);
	if (element != NULL)
		return NULL;

	element = malloc(sizeof *element);
    memcpy(element->hash, hash, SHA_DIGEST_LENGTH);
    element->ref_count = ref_count;
    element->block_index = block_index;
	HASH_ADD_PTR(*table, hash, element);
	return element;
}

int blocks_table_remove(blocks_hash_element_t **table, const unsigned char hash[SHA_DIGEST_LENGTH]) {
	blocks_hash_element_t *element;

	HASH_FIND_PTR(*table, hash, element);
	if (element == NULL)
		return 1;

	HASH_DEL(*table, element);
	free(element);
	return 0;
}

blocks_hash_element_t *blocks_table_find(blocks_hash_element_t *table, const unsigned char hash[SHA_DIGEST_LENGTH]) {
    blocks_hash_element_t *element;
    
	HASH_FIND_PTR(table, hash, element);
    return element;
}

void blocks_blocks_table_clear_foreach(blocks_hash_element_t *table, void (*func)(blocks_hash_element_t *)) {
	blocks_hash_element_t *curr, *tmp;

	HASH_ITER(hh, table, curr, tmp) {
		if (func != NULL) func(curr);
		HASH_DEL(table, curr);  /* delete; users advances to next */
		free(curr);             /* optional- if you want to free  */
	}
}

void blocks_table_clear(blocks_hash_element_t *table) {
	blocks_blocks_table_clear_foreach(table, NULL);
}

void blocks_table_print(blocks_hash_element_t *table, void (*print_func)(const char *format, ...)) {
    blocks_hash_element_t *s;

    for (s = table; s != NULL; s = s->hh.next) {
		print_func("hash ");
		for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
			print_func("%02x", s->hash[i]);
		print_func(": off %u, ref_count %u\n", s->block_index, s->ref_count);
    }
}