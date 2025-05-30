#ifndef FS_MANAGER_H
#define FS_MANAGER_H

#include <openssl/sha.h>
#include "list/list.h"
#include "hashtable/hashtable.h"

// Block repository related
#define BLOCK_BUFFER 1024

// General sizes
#define BLOCK_SIZE 4096
#define HASH_SIZE SHA_DIGEST_LENGTH

// User virtual file related
#define VIRTFILE_METADATA_SIZE 2
#define VIRTFILE_PTR_SIZE HASH_SIZE

// Metadata file related
#define METADATA_REF_COUNT_SIZE 4
#define METADATA_BLOCK_INDEX_SIZE 4
#define FRAGMENTATION_MAX_PERCENTAGE 0.1

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

/*  Types for defined size 
    NEED TO BE CHANGED SEPERATELY IN hashtable.h
*/
typedef unsigned int ref_count_t;
typedef unsigned int block_index_t;
typedef unsigned short block_offset_t;
typedef long block_count_t;

/* File descriptor for the blocks repository */
extern int blocks_fd, free_blocks_fd, metadata_fd;

/* List of free blocks */
extern list_t *free_blocks;

/* Metadata hash table */
extern hash_element_t *metadata;

/* ###################################################################################### */
/* ################################# LOAD/SAVE FUNCTIONS ################################ */
/* ###################################################################################### */

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

/* ###################################################################################### */
/* ############################# BLOCK REPOSITORY FUNCTIONS ############################# */
/* ###################################################################################### */

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
int add_reference_to_block(const unsigned char *buf, unsigned char hash[HASH_SIZE]);

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
int remove_reference_from_block(const unsigned char hash[HASH_SIZE]);

/*
    Checks if the fragmentation of the block repository file is above the allowed
    fragmentation percentage threshold and defragments it.

    Returns:
    SUCCESS - If defragmentation was complete or not necessary
    ERROR - If fstat fails
*/
int check_and_defragment_blocks();

/*
    Moves a block from the given src_index to the first
    free block on the repository.

    Returns:
    SUCCESS - on success
    ERROR - on error
*/
int copy_block_to_first_free(block_index_t src_index);

/* ###################################################################################### */
/* ############################### VIRTUAL FILE FUNCTIONS ############################### */
/* ###################################################################################### */

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
    Reads the metadata from a virtual file.
    
    Returns:
    the total read bytes - on success
    ERROR - on error
*/
ssize_t read_metadata_from_file(int fd, unsigned char buf[VIRTFILE_METADATA_SIZE]);

/*
    Write the metadata to a virtual file.
    
    Returns:
    the total read bytes - on success
    ERROR - on error
*/
ssize_t write_metadata_to_file(int fd, const unsigned char buf[VIRTFILE_METADATA_SIZE]);

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
ssize_t read_block_from_file(int fd, char *buf, block_index_t block_index, block_offset_t block_offset, short byte_count);

/*
    Read block_count full blocks from a file into a buffer starting at block start_index.

    Parameters:
    start_index - The position of the starting block inside the virtual file.
    block_count - The number of blocks to read.
    
    Returns:
    the total read bytes - on success 
    ERROR - on error
*/
ssize_t read_blocks_from_file(int fd, char *buf, block_index_t start_index, block_count_t block_count);

/*
    Write byte_count bytes to a file at a given block and offset. 
    This function assumes that the buf has a length that is at least BLOCK_SIZE and
    that it has enough size after offset for byte_count bytes to be read.

    Parameters:
    block_index - The position of the block's hash inside the virtual file.
    offset - The offset inside the block to start writing to. Range: [0, 4095]
    byte_count - The amount of bytes to write to the file. Range: [0, 4096]
    
    Returns:
    the total written bytes - on success 
    ERROR - on error
*/
ssize_t write_block_to_file(int fd, const char *buf, block_index_t block_index, block_offset_t block_offset, short byte_count);

/*
    Write block_count full blocks from a buffer into a file starting at block start_index.
    This function assumes that the buf has a length that is a multiple of BLOCK_SIZE and
    that it has enough size for block_count blocks to be read.

    Parameters:
    start_index - The position of the first block inside the virtual file.
    block_count - The number of blocks to write.
    
    Returns:
    the total written bytes - on success 
    ERROR - on error
*/
ssize_t write_blocks_to_file(int fd, const char *buf, block_index_t start_index, block_count_t block_count);

/*
    Adds zero padding to file until it reaches
    new_size bytes size.

    Returns:
    SUCCESS - on success
    ERROR - on error
*/
int zeropad_file(int fd, ssize_t new_size);

/*
    Shrinks the file to a total size of new_size bytes.
    If the new_size is larger than the current file size
    nothing is performed.

    Returns:
    SUCCESS - on success
    ERROR - on error
*/
int truncate_file(int fd, ssize_t new_size);

#endif