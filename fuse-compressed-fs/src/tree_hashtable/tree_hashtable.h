#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <limits.h>
#include <stdbool.h>
#include "../uthash.h"

#define TREE_HASH_ID_SIZE 37

typedef struct element {
    long long id[TREE_HASH_ID_SIZE];
    char name[NAME_MAX];
    bool is_dir;
    UT_hash_handle hh;
} tree_hash_element_t;

tree_hash_element_t *blocks_table_add(tree_hash_element_t **table, char name[NAME_MAX], bool is_dir);

tree_hash_element_t *blocks_table_find(tree_hash_element_t *table, char name[NAME_MAX]);

int blocks_table_remove(tree_hash_element_t **table, char name[NAME_MAX]);

void blocks_table_clear(tree_hash_element_t *table);

void blocks_table_print(tree_hash_element_t *table, void (*print_func)(const char *format, ...));

#endif