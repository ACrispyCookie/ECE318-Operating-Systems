#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <limits.h>
#include <stdbool.h>
#include "../uthash.h"

typedef struct element {
    unsigned long id;
    char name[NAME_MAX];
    bool is_dir;
    UT_hash_handle hh;
} nodes_hash_element_t;

nodes_hash_element_t *nodes_table_add(nodes_hash_element_t **table, char name[NAME_MAX], bool is_dir);

nodes_hash_element_t *nodes_table_find(nodes_hash_element_t *table, char name[NAME_MAX]);

int blocks_table_remove(nodes_hash_element_t **table, char name[NAME_MAX]);

void blocks_table_clear(nodes_hash_element_t *table);

void blocks_table_print(nodes_hash_element_t *table, void (*print_func)(const char *format, ...));

#endif