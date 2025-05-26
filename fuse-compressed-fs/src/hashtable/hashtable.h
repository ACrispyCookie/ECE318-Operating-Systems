#include "uthash.h"
#include <openssl/sha.h>

typedef struct element {
    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned int ref_count;
    unsigned int offset;
    UT_hash_handle hh;
} element_t;

int table_add(unsigned char hash[SHA_DIGEST_LENGTH], unsigned int ref_count, unsigned int offset);

element_t *table_find(unsigned char hash[SHA_DIGEST_LENGTH]);

int table_remove(unsigned char hash[SHA_DIGEST_LENGTH]);

void table_clear_foreach(void (*func)(element_t *));

void table_clear();

void table_print();