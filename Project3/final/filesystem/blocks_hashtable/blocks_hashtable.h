#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "../uthash.h"
#include <openssl/sha.h>

typedef struct blocks_element {
    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned int ref_count;
    unsigned int block_index;
    UT_hash_handle hh;
} blocks_hash_element_t;

blocks_hash_element_t *blocks_table_add(blocks_hash_element_t **table, const unsigned char hash[SHA_DIGEST_LENGTH], unsigned int ref_count, unsigned int offset);

blocks_hash_element_t *blocks_table_find(blocks_hash_element_t *table, const unsigned char hash[SHA_DIGEST_LENGTH]);

int blocks_table_remove(blocks_hash_element_t **table, const unsigned char hash[SHA_DIGEST_LENGTH]);

void blocks_table_clear_foreach(blocks_hash_element_t *table, void (*func)(blocks_hash_element_t *));

void blocks_table_clear(blocks_hash_element_t *table);

void blocks_table_print(blocks_hash_element_t *table, void (*print_func)(const char *format, ...));

#endif