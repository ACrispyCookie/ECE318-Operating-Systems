#ifndef NODES_HASHTABLE_H
#define NODES_HASHTABLE_H

#include <limits.h>
#include <stdbool.h>
#include "../uthash.h"

typedef struct nodes_element {
    char name[NAME_MAX];
    unsigned long id;
    struct nodes_element *hashmap;
    UT_hash_handle hh;
} nodes_hash_element_t;

nodes_hash_element_t *nodes_table_add_new(nodes_hash_element_t **table, const char name[NAME_MAX]);

nodes_hash_element_t *nodes_table_add(nodes_hash_element_t **table, const char name[NAME_MAX], unsigned long id);

nodes_hash_element_t *nodes_table_find(nodes_hash_element_t *table, const char name[NAME_MAX]);

int nodes_table_remove(nodes_hash_element_t **table, const char name[NAME_MAX]);

void nodes_table_clear_foreach(nodes_hash_element_t *table, void (*func)(nodes_hash_element_t *, int), int fd);

void nodes_table_clear(nodes_hash_element_t *table);

void set_last_id(unsigned long id);

unsigned long get_last_id();

#endif