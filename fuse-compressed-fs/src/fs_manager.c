#include "fs_manager.h"
#include "log.h"
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

/* File descriptor for the blocks repository */
int blocks_fd, free_blocks_fd, metadata_fd;

/* List of free blocks */
list_t *free_blocks;

/* Metadata hash table */
hash_element_t *metadata;

static void save_metadata_element(hash_element_t *element);
static void save_free_block(node_t *node);
static int offset_comparator(void *num1, void *num2);
static ssize_t get_block_hash(int fd, unsigned char hash[SHA_DIGEST_LENGTH], int block_offset);
static ssize_t get_block(char buf[BLOCK_SIZE], int block_offset, int byte_count);

void sha1_print(unsigned char hash[SHA_DIGEST_LENGTH]) {
    log_msg("SHA1 hash: ");
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        log_msg("%02x", hash[i]);
    }
    log_msg("\n");
}

/* ###################################################################################### */
/* ################################# INTERNAL FUNCTIONS ################################# */
/* ###################################################################################### */

/*
    Comparator for free_blocks list
*/
static int offset_comparator(void *num1, void *num2) {
    unsigned int offset1 = *((unsigned int *) num1);
    unsigned int offset2 = *((unsigned int *) num2);
    long int diff = (long int) offset1 - offset2;

    return (diff > 0) - (diff < 0);
}

/*
    Reads the block hash from a virtual file in the given offset.

    Parameters:
    block_offset - The offset inside the virtual file not counting the metadata size.
    
    Returns:
    the total read bytes - on success
    ERROR - on error
*/
static ssize_t get_block_hash(int fd, unsigned char hash[SHA_DIGEST_LENGTH], int block_offset) {
    int retstat = log_syscall("pread", pread(fd, hash, VIRTFILE_PTR_SIZE, VIRTFILE_METADATA_SIZE + block_offset), 0);
    return retstat < 0 ? ERROR : retstat;
}

/*
    Reads a block's content from the block repository.

    Parameters:
    block_offset - The offset inside the block repository.
    byte_count - The byte count to read from the block repository.
    
    Returns:
    the total read bytes - on success
    ERROR - on error
*/
static ssize_t get_block(char buf[BLOCK_SIZE], int block_offset, int byte_count) {
    int retstat = log_syscall("pread", pread(blocks_fd, buf, byte_count, block_offset), 0);
    return retstat < 0 ? ERROR : retstat;
}

/* ###################################################################################### */
/* ################################# LOAD/SAVE FUNCTIONS ################################ */
/* ###################################################################################### */

void load_metadata() 
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned int ref_count;
    unsigned int offset;

    while(1) {
        int read_res = log_syscall("read", read(metadata_fd, hash, HASH_SIZE), 0);
        if (read_res <= 0) 
            return;

        log_syscall("read", read(metadata_fd, &ref_count, METADATA_REF_COUNT_SIZE), 0);
        log_syscall("read", read(metadata_fd, &offset, METADATA_OFFSET_SIZE), 0);

        table_add(&metadata, hash, ref_count, offset);
    }
}

void load_free_blocks() 
{
    free_blocks = list_init(offset_comparator);

    while(1) {
        unsigned int *offset = malloc(sizeof(unsigned int));
        int read_res = log_syscall("read", read(free_blocks_fd, offset, METADATA_OFFSET_SIZE), 0);

        if (read_res <= 0)
            return;
        list_add(free_blocks, offset);
    }
}

void save_metadata() {
    log_syscall("ftruncate", ftruncate(metadata_fd, 0), 0);
    lseek(metadata_fd, 0, SEEK_SET);
    table_clear_foreach(metadata, save_metadata_element);
    close(metadata_fd);
}

void save_free_blocks() {
    log_syscall("ftruncate", ftruncate(free_blocks_fd, 0), 0);
    lseek(free_blocks_fd, 0, SEEK_SET);
    list_destroy_foreach(free_blocks, save_free_block);
    close(free_blocks_fd);
}

/*
    Saves a metadata entry in the metadata file.
*/
static void save_metadata_element(hash_element_t *element) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    memcpy(hash, element->hash, SHA_DIGEST_LENGTH);
    unsigned int ref_count = element->ref_count;
    unsigned int offset = element->offset;

    log_syscall("write", write(metadata_fd, hash, HASH_SIZE), 0);
    log_syscall("write", write(metadata_fd, &ref_count, METADATA_REF_COUNT_SIZE), 0);
    log_syscall("write", write(metadata_fd, &offset, METADATA_OFFSET_SIZE), 0);
}

/*
    Saves a free block in the free blocks file
*/
static void save_free_block(node_t *node) {
    unsigned int *offset = (unsigned int *) node->data;
    log_syscall("write", write(free_blocks_fd, offset, METADATA_OFFSET_SIZE), 0);
    free(offset);
}

/* ###################################################################################### */
/* ############################# BLOCK REPOSITORY FUNCTIONS ############################# */
/* ###################################################################################### */

int create_block(char *buf, unsigned char new_hash[SHA_DIGEST_LENGTH]) 
{
    int retstat;
    unsigned int new_block_offset;

    // Free block exists, remove it from list
    if (free_blocks->head->next != NULL) {
        new_block_offset = (*((unsigned int *) free_blocks->head->next->data)) * BLOCK_SIZE;
        list_remove_index(free_blocks, 0);
    } else { // No free block, get new offset at the end of file
        struct stat statbuf;
        retstat = fstat(blocks_fd, &statbuf);
        if (retstat < 0) return ERROR; 
        new_block_offset = statbuf.st_size;
    }

    // Add the hash of the new block in the hashtable
    retstat = log_syscall("pwrite", pwrite(blocks_fd, buf, BLOCK_SIZE, new_block_offset), 0);
    if (retstat < 0) return ERROR;
    hash_element_t *element = table_add(&metadata, new_hash, 1, new_block_offset);

    return SUCCESS;
}

int remove_block(unsigned char hash[SHA_DIGEST_LENGTH]) 
{
    hash_element_t *element = table_find(metadata, hash);
    if (element == NULL)
        return BLOCK_NOT_FOUND;
    
    unsigned int *block_offset = malloc(sizeof(unsigned int *));
    *block_offset = element->offset;
    
    table_remove(metadata, hash);
    list_add(free_blocks, block_offset);

    return BLOCK_FOUND;
}

int find_or_create_block(char *buf, unsigned char hash[SHA_DIGEST_LENGTH]) 
{
    SHA1(buf, BLOCK_SIZE, hash);

    hash_element_t *element = table_find(metadata, hash);
    if (element == NULL) {
        int retstat = create_block(buf, hash);
        return retstat < 0 ? ERROR : BLOCK_CREATED;
    }
    
    element->ref_count++;

    return BLOCK_FOUND;
}

/* ###################################################################################### */
/* ############################### VIRTUAL FILE FUNCTIONS ############################### */
/* ###################################################################################### */

ssize_t get_real_size(int fd) {
    struct stat statbuf;
    int retstat = log_syscall("lstat", fstat(fd, &statbuf), 0);
    return retstat < 0 ? ERROR : statbuf.st_size;
}

ssize_t get_total_size(int fd) {
    int last_block_size;
    int retstat = log_syscall("pread", pread(fd, &last_block_size, VIRTFILE_METADATA_SIZE, 0), 0);
    int size = last_block_size + ((get_real_size(fd) - VIRTFILE_METADATA_SIZE) / VIRTFILE_PTR_SIZE - 1) * BLOCK_SIZE;
    return retstat < 0 ? ERROR : MAX(size, 0);
}

ssize_t get_block_size(int fd) {
    return ((get_real_size(fd) - VIRTFILE_METADATA_SIZE) / VIRTFILE_PTR_SIZE) * BLOCK_SIZE;
}

ssize_t read_metadata(int fd, const char buf[VIRTFILE_METADATA_SIZE]) {
    int retstat = log_syscall("pread", pread(fd, buf, VIRTFILE_METADATA_SIZE, 0), 0);
    return retstat < 0 ? ERROR : retstat;
}

ssize_t write_metadata(int fd, const char buf[VIRTFILE_METADATA_SIZE]) {
    int retstat = log_syscall("pwrite", pwrite(fd, buf, VIRTFILE_METADATA_SIZE, 0), 0);
    return retstat < 0 ? ERROR : retstat;
}

ssize_t read_file_block(int fd, char *buf, unsigned int block_index, unsigned int block_offset, short int byte_count) {
    int retstat;
    if (byte_count <= 0)
        return 0;

    unsigned char hash[SHA_DIGEST_LENGTH];
    retstat = get_block_hash(fd, hash, block_index * VIRTFILE_PTR_SIZE);
    if (retstat < 0) return ERROR;

    int index = table_find(metadata, hash)->offset;
    retstat = get_block(buf, index * BLOCK_SIZE + block_offset, MIN(byte_count, BLOCK_SIZE - block_offset));
    if (retstat < 0) return ERROR;

    return retstat;
}

ssize_t read_file_blocks(int fd, char *buf, unsigned int start_index, int block_count) {
    int retstat;
    if (block_count <= 0)
        return 0;
    int total_read = 0;
    int virtual_file_offset = start_index * VIRTFILE_PTR_SIZE;

    for (int i = 0; i < block_count; i++, virtual_file_offset += VIRTFILE_PTR_SIZE) {
        unsigned char hash[SHA_DIGEST_LENGTH];
        retstat = get_block_hash(fd, hash, virtual_file_offset);
        if (retstat < 0) return ERROR;

        int index = table_find(metadata, hash)->offset;
        retstat = get_block(buf + total_read, index * BLOCK_SIZE, BLOCK_SIZE);
        if (retstat < 0) return ERROR;
        else total_read += retstat;
    }

    return total_read;
}
 
ssize_t zeropad_file(int fd, unsigned int start_offset, int block_count) {
    int retstat;
    if (block_count < 0)
        return 0;
    int offset = VIRTFILE_METADATA_SIZE + (start_offset / BLOCK_SIZE + (start_offset % BLOCK_SIZE == 0)) * VIRTFILE_PTR_SIZE;

    // Create zero block
    unsigned char hash[SHA_DIGEST_LENGTH];
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    retstat = find_or_create_block(buf, hash);
    if (retstat < 0) return ERROR;

    // Write zero blocks to the file
    int total_written = 0;
    for (int i = 0; i < block_count; i++, offset += VIRTFILE_PTR_SIZE) {
        retstat = log_syscall("pwrite", pwrite(fd, hash, VIRTFILE_PTR_SIZE, offset), 0);
        if (retstat <= 0)
            break;
        total_written += retstat;
    }

    return retstat < 0 ? ERROR : total_written;
}

int truncate_file(int fd, unsigned int new_size) {
    int retstat;
    int current_size = get_total_size(fd);
    if (current_size <= new_size)
        return SUCCESS;

    int last_block_size = new_size % BLOCK_SIZE;
    int offset = VIRTFILE_METADATA_SIZE + (new_size / BLOCK_SIZE + (last_block_size == 0)) * VIRTFILE_PTR_SIZE;
    int new_real_size = offset;

    // Write new last_block_size
    retstat = write_metadata(fd, (char *) &last_block_size);
    if (retstat < 0) return ERROR;

    while (1) {
        unsigned char hash[SHA_DIGEST_LENGTH];

        retstat = log_syscall("pread", pread(fd, hash, HASH_SIZE, offset), 0);
        if (retstat <= 0)
            break;
            
        remove_block(hash);
        offset += HASH_SIZE;
    }
    if (retstat < 0) return ERROR;
    
    retstat = ftruncate(fd, new_real_size);

    return retstat < 0 ? ERROR : SUCCESS;
}