#include "hashtable.h"
#include "log.h"
#include <stdio.h>

hash_element_t *table_add(hash_element_t **table, const unsigned char hash[SHA_DIGEST_LENGTH], unsigned int ref_count, unsigned int offset) {
	hash_element_t *element;

	HASH_FIND_PTR(*table, hash, element);
	if (element != NULL)
		return NULL;

	element = malloc(sizeof *element);
    memcpy(element->hash, hash, SHA_DIGEST_LENGTH);
    element->ref_count = ref_count;
    element->offset = offset;
	HASH_ADD_PTR(*table, hash, element);
	return element;
}

int table_remove(hash_element_t **table, const unsigned char hash[SHA_DIGEST_LENGTH]) {
	hash_element_t *element;

	HASH_FIND_PTR(*table, hash, element);
	if (element == NULL)
		return 1;

	HASH_DEL(*table, element);
	free(element);
	return 0;
}

hash_element_t *table_find(hash_element_t *table, const unsigned char hash[SHA_DIGEST_LENGTH]) {
    hash_element_t *element;
    
	HASH_FIND_PTR(table, hash, element);
    return element;
}

void table_clear_foreach(hash_element_t *table, void (*func)(hash_element_t *)) {
	hash_element_t *curr, *tmp;

	HASH_ITER(hh, table, curr, tmp) {
		if (func != NULL) func(curr);
		HASH_DEL(table, curr);  /* delete; users advances to next */
		free(curr);             /* optional- if you want to free  */
	}
}

void table_clear(hash_element_t *table) {
	table_clear_foreach(table, NULL);
}

void table_print(hash_element_t *table, void (*print_func)(const char *format, ...)) {
    hash_element_t *s;

    for (s = table; s != NULL; s = s->hh.next) {
		print_func("hash ");
		for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
			print_func("%02x", s->hash[i]);
		print_func(": off %u, ref_count %u\n", s->offset, s->ref_count);
    }
}

void table_foreach_run(hash_element_t *table, int (*func)(void *, const hash_element_t *), void *func_args, int (*comparator)(void *, void *), unsigned long long int limit) {
    hash_element_t *curr, *tmp;
    unsigned long long int iter = 0;

    HASH_ITER(hh, table, curr, tmp) {
        if (func != NULL)
			func(func_args, curr);

		if (comparator != NULL && limit && (comparator((void *)&iter, (void *)&limit) == 0))
			break;

        iter++;
    }
}
