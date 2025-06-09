#include "nodes_hashtable.h"
#include "log.h"
#include <limits.h>
#include <stdio.h>

unsigned long last_id = -1;

nodes_hash_element_t *nodes_table_add_new(nodes_hash_element_t **table, const char name[NAME_MAX + 1], bool is_dir) {
  	return nodes_table_add(table, name, is_dir, ++last_id);
}

nodes_hash_element_t *nodes_table_add(nodes_hash_element_t **table, const char name[NAME_MAX + 1], bool is_dir, unsigned long id) {
	nodes_hash_element_t *element;

	HASH_FIND_PTR(*table, name, element);
	if (element != NULL)
		return NULL;

	element = malloc(sizeof *element);
	element->id = id;
	strncpy(element->name, name, NAME_MAX);
	element->is_dir = is_dir;
	element->hashmap = NULL;
	HASH_ADD_PTR(*table, name, element);

	return element;
}

int nodes_table_remove(nodes_hash_element_t **table, const char name[NAME_MAX + 1]) {
	nodes_hash_element_t *element;

	HASH_FIND_PTR(*table, name, element);
	if (element == NULL)
		return 1;

	HASH_DEL(*table, element);
	free(element);

	return 0;
}

nodes_hash_element_t *nodes_table_find(nodes_hash_element_t *table, const char name[NAME_MAX + 1]) {
    nodes_hash_element_t *element;

	HASH_FIND_PTR(table, name, element);

    return element;
}

void print_dir(nodes_hash_element_t *element, int depth) {
	if (element == NULL)
		return;

	for (int i = 0; i < depth - 2; i++) log_msg(" ");
	log_msg("|-");
	log_msg("name: %s\n", element->name);
	for (int i = 0; i < depth - 2; i++) log_msg(" ");
	log_msg("| ");
	log_msg("element: %p\n", element);
	for (int i = 0; i < depth - 2; i++) log_msg(" ");
	log_msg("| ");
	log_msg("hashmap: %p\n", element->hashmap);
	for (int i = 0; i < depth - 2; i++) log_msg(" ");
	log_msg("| ");
	log_msg("id: %lu\n", element->id);
	for (int i = 0; i < depth - 2; i++) log_msg(" ");
	log_msg("| ");
	log_msg("is_dir: %d\n", element->is_dir);
	if (element->is_dir)
		print_dir(element->hashmap, depth + 2);
}

void nodes_table_print(nodes_hash_element_t *table) {
	nodes_hash_element_t *element;

	log_msg("###########################################################\n");
	for (element = table; element != NULL; element = element->hh.next) {
		print_dir(element, 2);
	}
	log_msg("###########################################################\n");
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
