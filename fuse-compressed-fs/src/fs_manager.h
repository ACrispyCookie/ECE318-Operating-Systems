#include <openssl/sha.h>
#include "list/list.h"
#include "hashtable/hashtable.h"

// General sizes
#define BLOCK_SIZE 4096
#define HASH_SIZE 20

// User virtual file related
#define VIRTFILE_METADATA_SIZE 2
#define VIRTFILE_PTR_SIZE HASH_SIZE

// Metadata file related
#define METADATA_REF_COUNT_SIZE 4
#define METADATA_OFFSET_SIZE 4
#define METADATA_FILE_ENTRY_SIZE (HASH_SIZE + METADATA_REF_COUNT_SIZE + METADATA_OFFSET_SIZE)

// File related
#define BLOCKS_PATH "/blocks"
#define FREE_BLOCKS_PATH "/free_blocks"
#define METADATA_PATH "/metadata"
#define USER_PATH "/user"
#define STORAGE_FILES_PERMISSIONS 0664

// Return codes
#define ERROR -1
#define SUCCESS 0
#define BLOCK_CREATED 1
#define BLOCK_FOUND 0
#define BLOCK_NOT_FOUND 1

// Math helpers
#define CEIL_TO_MULT(x, n)  (((x) + (n - 1)) & ~(n - 1))
#define MIN(x, y) x > y ? y : x
#define MAX(x, y) x > y ? x : y

/* File descriptor for the blocks repository */
int blocks_fd, free_blocks_fd, metadata_fd;

/* List of free blocks */
list_t *free_blocks;

/* 
    Creates a block and stores it in the block 
    repository, generates the metadata for the block and adds it
    to the metadata hashmap.

    The reference count on the new metadata entry is set to 1.

    Returns:
    SUCCESS - on success
    ERROR - on error
*/
int create_block(char *buf, char new_hash[SHA_DIGEST_LENGTH]);

/*
    Deletes the metadata entry for a given block if it exists,
    adds it to the free blocks list and if needed performs defragmentation.
    
    Returns:
    BLOCK_FOUND - If the block was found
    BLOCK_NOT_FOUND - If the block wasn't found
*/
int remove_block(char hash[SHA_DIGEST_LENGTH]);

/*
    Tries to find a block with the given hash and if it
    fails it creates a new one adding it to the block repository.

    If the block already existed it increments the reference count
    in its metadata entry.

    Returns:
    BLOCK_CREATED - If the block was created
    BLOCK_FOUND - If the block was found inside the repository
    ERROR - on error
*/
int find_or_create_block(char *buf, char hash[SHA_DIGEST_LENGTH]);

/*
    Get real size of a virtual file.
    (Sum of all the pointers to blocks and the metadata)

    Returns:
    number of bytes - on success
    ERROR - on error
*/
ssize_t get_real_size(int fd);

/*
    Get total size of a virtual file.
    (Virtual file size shown to user)

    Returns:
    number of bytes - on success
    ERROR - on error
*/
ssize_t get_total_size(int fd);

/*
    Get block size of a virtual file.
    (Sum of the sizes of all the blocks in the file)

    Returns:
    number of bytes - on success
    ERROR - on error
*/
ssize_t get_block_size(int fd);

/*
    Read block_count full blocks from a file into a buffer.
    
    Returns:
    the total written bytes - on success 
    ERROR - on error
*/
ssize_t read_file_blocks(int fd, unsigned int start_index, char *buf, int block_count);

/*
    Adds block_count number of blocks that contain zero, starting at
    position start_index * BLOCK_SIZE inside the file.

    Returns:
    the total written bytes - on success 
    ERROR - on error
*/
ssize_t zeropad_file(int fd, unsigned int start_offset, int block_count);

/*
    Shrinks the file to a total size of new_size bytes.
    If the new_size is larger than the current file size
    nothing is performed.

    Returns:
    SUCCESS - on success
    ERROR - on error
*/
int truncate_file(int fd, unsigned int new_size);