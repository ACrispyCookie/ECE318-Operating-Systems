#ifndef FS_MANAGER_H
#define FS_MANAGER_H

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
#define DEFRAGMENT_MAX_UNUSED 2

// File related
#define BLOCKS_PATH "/blocks"
#define FREE_BLOCKS_PATH "/free_blocks"
#define METADATA_PATH "/metadata"
#define USER_PATH "/user"
#define STORAGE_FILES_PERMISSIONS 0664
#define USER_FOLDER_PERMISSIONS 0774

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
extern int blocks_fd, free_blocks_fd, metadata_fd;

/* List of free blocks */
extern list_t *free_blocks;

/* Metadata hash table */
extern hash_element_t *metadata;

/*
    Load the metadata hashtable from the metadata file.
*/
void load_metadata();

/*
    Load the free blocks list from the free blocks file.
*/
void load_free_blocks();

/*
    Saves the metadata hashtable to the metadata file and destroys it
*/
void save_metadata();

/*
    Saves the free block list in the free blocks files and clears it.
*/
void save_free_blocks();

/* 
    Creates a block and stores it in the block 
    repository, generates the metadata for the block and adds it
    to the metadata hashmap.

    The reference count on the new metadata entry is set to 1.

    Returns:
    SUCCESS - on success
    ERROR - on error
*/
int create_block(const unsigned char *buf, unsigned char new_hash[SHA_DIGEST_LENGTH]);

/*
    Deletes the metadata entry for a given block if it exists,
    adds it to the free blocks list and if needed performs defragmentation.
    
    Returns:
    BLOCK_FOUND - If the block was found
    BLOCK_NOT_FOUND - If the block wasn't found
*/
int remove_block(const unsigned char hash[SHA_DIGEST_LENGTH]);

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
int add_reference_to_block(const unsigned char *buf, unsigned char hash[SHA_DIGEST_LENGTH]);

/*
    Tries to find a block with the given hash and if it
    fails it creates a new one adding it to the block repository.

    If the block already existed it increments the reference count
    in its metadata entry.

    Returns:
    BLOCK_FOUND - If the block was found and the reference count was updated.
    BLOCK_NOT_FOUND - If the block was not found.
    ERROR - on error
*/
int remove_reference_from_block(const unsigned char hash[SHA_DIGEST_LENGTH]);

/*
    Get real size of a virtual file.
    (Sum of all the pointers to blocks and the metadata)

    Returns:
    number of bytes - on success
    ERROR - on error
*/
ssize_t get_virtual_file_size(int fd);

/*
    Get total size of a virtual file.
    (Virtual file size shown to user)

    Returns:
    number of bytes - on success
    ERROR - on error
*/
ssize_t get_user_file_size(int fd);

/*
    Reads the metadata of a virtual file.
    
    Returns:
    the total read bytes - on success
    ERROR - on error
*/
ssize_t read_metadata(int fd, char buf[VIRTFILE_METADATA_SIZE]);

/*
    Write the metadata of a virtual file.
    
    Returns:
    the total read bytes - on success
    ERROR - on error
*/
ssize_t write_metadata(int fd, const char buf[VIRTFILE_METADATA_SIZE]);

/*
    Read block_count full blocks from a file into a buffer starting at block start_index.

    Parameters:
    start_index - The position of the starting block inside the virtual file.
    block_count - The number of blocks to read.
    
    Returns:
    the total read bytes - on success 
    ERROR - on error
*/
ssize_t read_file_blocks(int fd, char *buf, unsigned int start_index, int block_count);

/*
    Read byte_count bytes from a file at a given block and offset.

    Parameters:
    block_index - The position of the block's hash inside the virtual file.
    offset - The offset inside the block to start reading from. Range: [0, 4095]
    byte_count - The amount of bytes to read from the block. Range: [0, 4096]
    
    Returns:
    the total read bytes - on success 
    ERROR - on error
*/
ssize_t read_file_block(int fd, char *buf, unsigned int block_index, unsigned int offset, short int byte_count);

/*
    Write block_count full blocks from a buffer into a file starting at block start_index.
    This function assumes that the buf has a length that is a multiple of BLOCK_SIZE and
    that it has enough size for block_count blocks to be read.

    Parameters:
    start_index - The position of the first block inside the virtual file.
    block_count - The number of blocks to write.
    
    Returns:
    the total write bytes - on success 
    ERROR - on error
*/
ssize_t write_file_blocks(int fd, const char *buf, unsigned int start_index, int block_count);

/*
    Write byte_count bytes to a file at a given block and offset. 
    This function assumes that the buf has a length that is at least BLOCK_SIZE and
    that it has enough size after offset for byte_count bytes to be read.

    Parameters:
    block_index - The position of the block's hash inside the virtual file.
    offset - The offset inside the block to start writing to. Range: [0, 4095]
    byte_count - The amount of bytes to write to the file. Range: [0, 4096]
    
    Returns:
    the total read bytes - on success 
    ERROR - on error
*/
ssize_t write_file_block(int fd, const char *buf, unsigned int block_index, unsigned int offset, short int byte_count);

/*
    Adds zero padding to file until it reaches
    new_size bytes size.

    Returns:
    SUCCESS - on success
    ERROR - on error
*/
int zeropad_file(int fd, unsigned int new_size);

/*
    Shrinks the file to a total size of new_size bytes.
    If the new_size is larger than the current file size
    nothing is performed.

    Returns:
    SUCCESS - on success
    ERROR - on error
*/
int truncate_file(int fd, unsigned int new_size);

#endif