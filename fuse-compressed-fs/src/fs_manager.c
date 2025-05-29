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

static int offset_comparator(void *num1, void *num2);
static ssize_t read_block_hash(int fd, unsigned char hash[SHA_DIGEST_LENGTH], int block_offset);
static ssize_t write_block_hash(int fd, unsigned char hash[SHA_DIGEST_LENGTH], int block_offset);
static ssize_t read_block(unsigned char buf[BLOCK_SIZE], int block_offset, int byte_count);
static void save_metadata_element(hash_element_t *element);
static void save_free_block(node_t *node);

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
    block_offset - The offset inside the virtual file including the metadata size.
    
    Returns:
    the total read bytes - on success
    ERROR - on error
*/
static ssize_t read_block_hash(int fd, unsigned char hash[SHA_DIGEST_LENGTH], int block_offset) {
    int retstat = log_syscall("pread", pread(fd, hash, VIRTFILE_PTR_SIZE, block_offset), 0);
    return retstat < 0 ? ERROR : retstat;
}

/*
    Writes the block hash to a virtual file in the given offset.

    Parameters:
    block_offset - The offset inside the virtual file including the metadata size.
    
    Returns:
    the total write bytes - on success
    ERROR - on error
*/
static ssize_t write_block_hash(int fd, unsigned char hash[SHA_DIGEST_LENGTH], int block_offset) {
    int retstat = log_syscall("pwrite", pwrite(fd, hash, VIRTFILE_PTR_SIZE, block_offset), 0);
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
static ssize_t read_block(unsigned char buf[BLOCK_SIZE], int block_offset, int byte_count) {
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
    unsigned long int offset;

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
        unsigned long int *offset = malloc(METADATA_OFFSET_SIZE);
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
    unsigned long int offset = element->offset;

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

int create_block(const unsigned char *buf, unsigned char new_hash[SHA_DIGEST_LENGTH]) 
{
    int retstat;
    unsigned int new_block_offset;

    // Free block exists, remove it from list
    if (free_blocks->head->next != free_blocks->head) {
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
    table_add(&metadata, new_hash, 1, new_block_offset / BLOCK_SIZE);

    return SUCCESS;
}

int remove_block(const unsigned char hash[SHA_DIGEST_LENGTH]) 
{
    hash_element_t *element = table_find(metadata, hash);
    if (element == NULL)
        return BLOCK_NOT_FOUND;
    
    unsigned int *block_offset = malloc(sizeof(unsigned int *));
    *block_offset = element->offset;
    
    table_remove(&metadata, hash);
    list_add(free_blocks, block_offset);

    return BLOCK_FOUND;
}

int add_reference_to_block(const unsigned char *buf, unsigned char hash[SHA_DIGEST_LENGTH]) 
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

int remove_reference_from_block(const unsigned char hash[SHA_DIGEST_LENGTH]) 
{
    hash_element_t *element = table_find(metadata, hash);
    if (element == NULL)
        return BLOCK_NOT_FOUND;
    
    if (element->ref_count == 0)
        return ERROR;
    
    element->ref_count--;
    if (element->ref_count == 0)
        remove_block(hash);

    return BLOCK_FOUND;
}

/* ###################################################################################### */
/* ############################### VIRTUAL FILE FUNCTIONS ############################### */
/* ###################################################################################### */

ssize_t get_virtual_file_size(int fd) {
    struct stat statbuf;
    int retstat = log_syscall("lstat", fstat(fd, &statbuf), 0);
    return retstat < 0 ? ERROR : statbuf.st_size;
}

ssize_t get_user_file_size(int fd) {
    unsigned short int last_block_size;
    ssize_t retstat = read_metadata(fd, (char *) &last_block_size);
    ssize_t virtual_file_size = get_virtual_file_size(fd);
    ssize_t size = last_block_size + ((virtual_file_size - VIRTFILE_METADATA_SIZE) / VIRTFILE_PTR_SIZE - (last_block_size != 0)) * BLOCK_SIZE;
    return retstat < 0 ? ERROR : MAX(size, 0);
}

ssize_t read_metadata(int fd, char buf[VIRTFILE_METADATA_SIZE]) {
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
    retstat = read_block_hash(fd, hash, VIRTFILE_METADATA_SIZE + block_index * VIRTFILE_PTR_SIZE);
    if (retstat < 0) return ERROR;

    int index = table_find(metadata, hash)->offset;
    retstat = read_block((unsigned char *) buf, index * BLOCK_SIZE + block_offset, MIN(byte_count, BLOCK_SIZE - block_offset));
    if (retstat < 0) return ERROR;

    return retstat;
}

ssize_t read_file_blocks(int fd, char *buf, unsigned int start_index, int block_count) {
    int retstat;
    if (block_count <= 0)
        return 0;
    int total_read = 0;
    int virtual_file_offset = VIRTFILE_METADATA_SIZE + start_index * VIRTFILE_PTR_SIZE;

    for (int i = 0; i < block_count; i++, virtual_file_offset += VIRTFILE_PTR_SIZE) {
        unsigned char hash[SHA_DIGEST_LENGTH];
        retstat = read_block_hash(fd, hash, virtual_file_offset);
        if (retstat < 0) return ERROR;

        int index = table_find(metadata, hash)->offset;
        retstat = read_block((unsigned char *) buf + total_read, index * BLOCK_SIZE, BLOCK_SIZE);
        if (retstat < 0) return ERROR;
        else total_read += retstat;
    }

    return total_read;
}

ssize_t write_file_block(int fd, const char *buf, unsigned int block_index, unsigned int block_offset, short int byte_count) {
    int retstat, total_written = 0;
    if (byte_count <= 0)
        return 0;

    unsigned char new_block[BLOCK_SIZE];
    unsigned char old_hash[SHA_DIGEST_LENGTH], new_hash[SHA_DIGEST_LENGTH];
    int virtual_file_offset = VIRTFILE_METADATA_SIZE + block_index * VIRTFILE_PTR_SIZE;
    int old_block_exists;

    // Read current block contents in the current block_index 
    retstat = read_block_hash(fd, old_hash, virtual_file_offset);
    if (retstat < 0) return ERROR;

    // Read previous block content or fill it with zeros
    old_block_exists = (retstat != 0);
    if (old_block_exists) {
        int index = table_find(metadata, old_hash)->offset;
        retstat = read_block(new_block, index * BLOCK_SIZE, BLOCK_SIZE);
        if (retstat < 0) return ERROR;
    } else {
        memset(new_block, 0, BLOCK_SIZE);
    }

    // Write changes to a temporary new_block buffer
    total_written = MIN(byte_count, BLOCK_SIZE - block_offset);
    memcpy(new_block + block_offset, buf, total_written);

    // Add new block and remove old block if it existed
    retstat = add_reference_to_block((unsigned char *) new_block, new_hash);
    if (retstat < 0) return ERROR;

    if (old_block_exists) {
        retstat = remove_reference_from_block(old_hash);
        if (retstat < 0) return ERROR;
    }

    // Write new block hash if it's different back to the file
    if (memcmp(old_hash, new_hash, SHA_DIGEST_LENGTH) == 0)
        return byte_count;

    retstat = write_block_hash(fd, new_hash, virtual_file_offset);
    if (retstat < 0) return ERROR;

    return byte_count;
}

ssize_t write_file_blocks(int fd, const char *buf, unsigned int start_index, int block_count) {
    int retstat;
    if (block_count <= 0)
        return 0;
    int virtual_file_offset = VIRTFILE_METADATA_SIZE + start_index * VIRTFILE_PTR_SIZE;

    for (int i = 0; i < block_count; i++, virtual_file_offset += VIRTFILE_PTR_SIZE) {
        unsigned char old_hash[SHA_DIGEST_LENGTH], new_hash[SHA_DIGEST_LENGTH];
        retstat = read_block_hash(fd, old_hash, virtual_file_offset);
        if (retstat < 0) return ERROR;
        
        retstat = add_reference_to_block((unsigned char *) buf, new_hash);
        if (retstat < 0) return ERROR;

        retstat = remove_reference_from_block(old_hash);
        if (retstat < 0) return ERROR;

        if (memcmp(old_hash, new_hash, SHA_DIGEST_LENGTH) == 0)
            continue;

        retstat = write_block_hash(fd, new_hash, virtual_file_offset);
        if (retstat < 0) return ERROR;
    }

    return block_count * BLOCK_SIZE;
}
 
int zeropad_file(int fd, unsigned int new_size) {
    int retstat = SUCCESS;
    ssize_t file_size = get_user_file_size(fd);
    if (file_size == ERROR) return ERROR;
    if (file_size >= new_size) return SUCCESS;

    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned char buf[BLOCK_SIZE];

    int last_block_index = MAX((get_virtual_file_size(fd) - VIRTFILE_PTR_SIZE - VIRTFILE_METADATA_SIZE) / VIRTFILE_PTR_SIZE, 0);
    unsigned short int last_block_size;
    unsigned short int new_last_block_size = new_size % BLOCK_SIZE;

    // Read old last block size and update it
    retstat = read_metadata(fd, (char *) &last_block_size);
    if (retstat == ERROR) return ERROR;
    retstat = write_metadata(fd, (char *) &new_last_block_size);
    if (retstat == ERROR) return ERROR;
    
    // Fill the first block with zeros
    memset(buf, 0, BLOCK_SIZE);
    retstat = write_file_block(fd, (char *) buf, last_block_index, last_block_size, (BLOCK_SIZE - last_block_size) % BLOCK_SIZE);
    if (retstat == ERROR) return -1;

    // If not other zero blocks are needed
    int bytes_to_pad = MAX(new_size - (file_size + BLOCK_SIZE - last_block_size), 0);
    int zero_blocks = bytes_to_pad / BLOCK_SIZE + (bytes_to_pad % BLOCK_SIZE != 0);
    if (zero_blocks == 0)
        return SUCCESS;

    // Create zero block
    memset(buf, 0, BLOCK_SIZE);
    retstat = add_reference_to_block(buf, hash);
    if (retstat == ERROR) return ERROR;

    // Write zero blocks to the file
    ssize_t offset = get_virtual_file_size(fd);
    for (int i = 0; i < zero_blocks; i++, offset += VIRTFILE_PTR_SIZE) {
        retstat = write_block_hash(fd, hash, offset);
        if (retstat <= 0)
            break;
    }

    hash_element_t *zero_block_hash = table_find(metadata, hash);
    zero_block_hash->ref_count += zero_blocks - 1;

    return retstat < 0 ? ERROR : SUCCESS;
}

int truncate_file(int fd, unsigned int new_size) {
    int retstat = SUCCESS;
    ssize_t current_size = get_user_file_size(fd);
    if (current_size <= new_size)
        return zeropad_file(fd, new_size);

    unsigned int last_block_size = new_size % BLOCK_SIZE;
    unsigned int new_real_size = VIRTFILE_METADATA_SIZE + (new_size / BLOCK_SIZE + (last_block_size != 0)) * VIRTFILE_PTR_SIZE;
    unsigned int offset = new_real_size;

    // Write new last_block_size
    retstat = write_metadata(fd, (char *) &last_block_size);
    if (retstat == ERROR) return ERROR;

    while (1) {
        unsigned char hash[SHA_DIGEST_LENGTH];

        retstat = read_block_hash(fd, hash, offset);
        if (retstat <= 0)
            break;
            
        remove_reference_from_block(hash);
        offset += VIRTFILE_PTR_SIZE;
    }
    if (retstat < 0) return ERROR;
    
    // Truncate virtual file to new size
    retstat = ftruncate(fd, new_real_size);

    return retstat < 0 ? ERROR : SUCCESS;
}