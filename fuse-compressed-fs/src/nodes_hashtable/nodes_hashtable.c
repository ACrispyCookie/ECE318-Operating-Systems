#include "tree_hashtable.h"
#include "log.h"
#include <stdio.h>

unsigned long last_id = 0;

nodes_hash_element_t *nodes_table_add(nodes_hash_element_t **table, char name[NAME_MAX], bool is_dir, unsigned long id) {
	nodes_hash_element_t *element;

	HASH_FIND_PTR(*table, hash, element);
	if (element != NULL)
		return NULL;

	element = malloc(sizeof *element);

	element->id = last_id;
	strncpy(element->name, name, NAME_MAX);
	element->is_dir = is_dir;

	return element;
}

nodes_hash_element_t *nodes_table_add_new(nodes_hash_element_t **table, const char name[NAME_MAX], bool is_dir) {
	last_id++;

  	return nodes_table_add(table, name, is_dir, last_id);
}

int nodes_table_remove(nodes_hash_element_t **table, unsigned long id) {
	nodes_hash_element_t *element;

	HASH_FIND_PTR(*table, id, element);
	if (element == NULL)
		return 1;

	HASH_DEL(*table, element);
	free(element);

	return 0;
}

nodes_hash_element_t *nodes_table_find(nodes_hash_element_t *table, const char name[NAME_MAX]) {
    nodes_hash_element_t *element;

	HASH_FIND_PTR(table, name, element);

    return element;
}

void nodes_table_clear_foreach(nodes_hash_element_t *table, void (*func)(nodes_hash_element_t *)) {
	nodes_hash_element_t *curr, *tmp;

	HASH_ITER(hh, table, curr, tmp) {
		if (func != NULL)
			func(curr);

		HASH_DEL(table, curr);
		free(curr);
	}
}

void nodes_table_clear(nodes_hash_element_t *table) {
	nodes_table_clear_foreach(table, NULL);
}
