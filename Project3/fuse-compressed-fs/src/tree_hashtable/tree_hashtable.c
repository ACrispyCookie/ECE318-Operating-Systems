#include "tree_hashtable.h"
#include "log.h"
#include <stdio.h>

int last_id = 0;

tree_hash_element_t *tree_table_add(tree_hash_element_t **table, char name[NAME_MAX], bool is_dir, int last_id) {
	tree_hash_element_t *element;

	HASH_FIND_PTR(*table, hash, element);
	if (element != NULL)
		return NULL;

	element = malloc(sizeof *element);

	element->id = last_id;
	strncpy(element->name, name, NAME_MAX);
	element->is_dir = is_dir;

	return element;
}

tree_hash_element_t *tree_table_add_new(tree_hash_element_t **table, char name[NAME_MAX], bool is_dir) {
	last_id++;

  	return tree_table_add(table, name, is_dir, last_id);
}

int tree_table_remove(tree_hash_element_t **table, const unsigned char hash[SHA_DIGEST_LENGTH]) {
	tree_hash_element_t *element;

	HASH_FIND_PTR(*table, hash, element);
	if (element == NULL)
		return 1;

	HASH_DEL(*table, element);
	free(element);

	return 0;
}

tree_hash_element_t *tree_table_find(tree_hash_element_t *table, const unsigned char hash[SHA_DIGEST_LENGTH]) {
    tree_hash_element_t *element;
    
	HASH_FIND_PTR(table, hash, element);

    return element;
}

void tree_table_clear_foreach(tree_hash_element_t *table, void (*func)(tree_hash_element_t *)) {
	tree_hash_element_t *curr, *tmp;

	HASH_ITER(hh, table, curr, tmp) {
		if (func != NULL)
			func(curr);

		HASH_DEL(table, curr);
		free(curr);
	}
}

void tree_table_clear(tree_hash_element_t *table) {
	tree_table_clear_foreach(table, NULL);
}
