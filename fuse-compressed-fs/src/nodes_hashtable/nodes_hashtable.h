#ifndef NODES_HASHTABLE_H
#define NODES_HASHTABLE_H

#include <limits.h>
#include <stdbool.h>
#include "../uthash.h"

typedef struct nodes_element {
    char name[NAME_MAX + 1];
    unsigned long id;
    bool is_dir;
    struct nodes_element * hashmap;
    UT_hash_handle hh;
} nodes_hash_element_t;

nodes_hash_element_t *nodes_table_add_new(nodes_hash_element_t **table, const char name[NAME_MAX + 1], bool is_dir);

nodes_hash_element_t *nodes_table_add(nodes_hash_element_t **table, const char name[NAME_MAX + 1], bool is_dir, unsigned long id);

int nodes_table_add_element(nodes_hash_element_t **table, nodes_hash_element_t *element);

nodes_hash_element_t *nodes_table_find(nodes_hash_element_t *table, const char name[NAME_MAX + 1]);

int nodes_table_remove(nodes_hash_element_t **table, const char name[NAME_MAX + 1]);

int nodes_table_remove_element(nodes_hash_element_t **table, nodes_hash_element_t *element);

void nodes_table_print(nodes_hash_element_t *table);

void nodes_table_clear_foreach(nodes_hash_element_t *table, void (*func)(nodes_hash_element_t *, int), int fd);

void nodes_table_clear(nodes_hash_element_t *table);

void set_last_id(unsigned long id);

unsigned long get_last_id();

#endif