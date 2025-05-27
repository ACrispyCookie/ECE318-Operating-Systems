#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "uthash.h"
#include <openssl/sha.h>

typedef struct element {
    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned int ref_count;
    unsigned int offset;
    UT_hash_handle hh;
} hash_element_t;

hash_element_t *table_add(hash_element_t **table, unsigned char hash[SHA_DIGEST_LENGTH], unsigned int ref_count, unsigned int offset);

hash_element_t *table_find(hash_element_t *table, unsigned char hash[SHA_DIGEST_LENGTH]);

int table_remove(hash_element_t *table, unsigned char hash[SHA_DIGEST_LENGTH]);

void table_clear_foreach(hash_element_t *table, void (*func)(hash_element_t *));

void table_clear(hash_element_t *table);

void table_print(hash_element_t *table, void (*print_func)(const char *format, ...));

#endif