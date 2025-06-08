#include "nodes_hashtable.h"
#include "log.h"
#include <limits.h>
#include <stdio.h>

unsigned long last_id = -1;

nodes_hash_element_t *nodes_table_add_new(nodes_hash_element_t **table, const char name[NAME_MAX]) {
  	return nodes_table_add(table, name, ++last_id);
}

nodes_hash_element_t *nodes_table_add(nodes_hash_element_t **table, const char name[NAME_MAX], unsigned long id) {
	nodes_hash_element_t *element;

	HASH_FIND_PTR(*table, name, element);
	if (element != NULL)
		return NULL;

	element = malloc(sizeof *element);

	element->id = id;
	strncpy(element->name, name, NAME_MAX);
	element->hashmap = NULL;

	return element;
}

int nodes_table_remove(nodes_hash_element_t **table, const char name[NAME_MAX]) {
	nodes_hash_element_t *element;

	HASH_FIND_PTR(*table, name, element);
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

void nodes_table_clear_foreach(nodes_hash_element_t *table, void (*func)(nodes_hash_element_t *, int), int fd) {
	nodes_hash_element_t *curr, *tmp;

	HASH_ITER(hh, table, curr, tmp) {
		if (func != NULL)
			func(curr, fd);

		HASH_DEL(table, curr);
		free(curr);
	}
}

void nodes_table_clear(nodes_hash_element_t *table) {
	nodes_table_clear_foreach(table, NULL, -1);
}

void set_last_id(unsigned long id) {
	last_id = id;
}

unsigned long get_last_id() {
	return last_id;
}
