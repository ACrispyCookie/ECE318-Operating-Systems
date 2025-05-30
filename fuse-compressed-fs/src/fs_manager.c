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

void sha1_print(unsigned char hash[HASH_SIZE]) {
    log_msg("SHA1 hash: ");
    for (int i = 0; i < HASH_SIZE; i++) {
        log_msg("%02x", hash[i]);
    }
    log_msg("\n");
}

/*
    Comparator for free_blocks list
*/
static int index_comparator(void *num1, void *num2);

/*
    Reads the block hash from a virtual file in the given offset.

    Parameters:
    block_offset - The offset inside the virtual file including the metadata size.
    
    Returns:
    the total read bytes - on success
    ERROR - on error
*/
static ssize_t read_hash_from_file(int fd, unsigned char *hash, off_t block_offset, ssize_t count);

/*
    Writes the block hash to a virtual file in the given offset.

    Parameters:
    block_offset - The offset inside the virtual file including the metadata size.
    
    Returns:
    the total write bytes - on success
    ERROR - on error
*/
static ssize_t write_hash_to_file(int fd, unsigned char *hash, off_t block_offset, ssize_t count);

/*
    Reads a block's content from the block repository.

    Parameters:
    block_offset - The offset inside the block repository.
    byte_count - The byte count to read from the block repository.
    
    Returns:
    the total read bytes - on success
    ERROR - on error
*/
static ssize_t read_block(unsigned char buf[BLOCK_SIZE], off_t block_offset, ssize_t byte_count);

/*
    Saves a metadata entry in the metadata file.
*/
static void save_metadata_element(hash_element_t *element);

/*
    Saves a free block in the free blocks file
*/
static void save_free_block(node_t *node);

/* ###################################################################################### */
/* ################################# INTERNAL FUNCTIONS ################################# */
/* ###################################################################################### */

static int index_comparator(void *num1, void *num2) {
    block_index_t index1 = *((block_index_t *) num1);
    block_index_t index2 = *((block_index_t *) num2);
    long int diff = (long int) index1 - index2;

    return (diff > 0) - (diff < 0);
}

/* ###################################################################################### */
/* ################################# LOAD/SAVE FUNCTIONS ################################ */
/* ###################################################################################### */

void load_metadata() 
{
    unsigned char hash[HASH_SIZE];
    ref_count_t ref_count;
    block_index_t block_index;

    while(1) {
        int read_res = log_syscall("read", read(metadata_fd, hash, HASH_SIZE), 0);
        if (read_res <= 0) 
            return;

        log_syscall("read", read(metadata_fd, &ref_count, METADATA_REF_COUNT_SIZE), 0);
        log_syscall("read", read(metadata_fd, &block_index, METADATA_BLOCK_INDEX_SIZE), 0);

        table_add(&metadata, hash, ref_count, block_index);
    }
}

void load_free_blocks() 
{
    free_blocks = list_init(index_comparator);

    while(1) {
        block_index_t *block_index = malloc(sizeof(block_index_t));
        int read_res = log_syscall("read", read(free_blocks_fd, block_index, METADATA_BLOCK_INDEX_SIZE), 0);

        if (read_res <= 0)
            return;
        list_add(free_blocks, block_index);
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

static void save_metadata_element(hash_element_t *element) {
    unsigned char hash[HASH_SIZE];
    memcpy(hash, element->hash, HASH_SIZE);
    ref_count_t ref_count = element->ref_count;
    block_index_t block_index = element->block_index;

    log_syscall("write", write(metadata_fd, hash, HASH_SIZE), 0);
    log_syscall("write", write(metadata_fd, &ref_count, METADATA_REF_COUNT_SIZE), 0);
    log_syscall("write", write(metadata_fd, &block_index, METADATA_BLOCK_INDEX_SIZE), 0);
}

static void save_free_block(node_t *node) {
    block_index_t *block_index = (block_index_t *) node->data;
    log_syscall("write", write(free_blocks_fd, block_index, METADATA_BLOCK_INDEX_SIZE), 0);
    free(block_index);
}

/* ###################################################################################### */
/* ############################# BLOCK REPOSITORY FUNCTIONS ############################# */
/* ###################################################################################### */

int create_block(const unsigned char *buf, unsigned char new_hash[HASH_SIZE]) 
{
    int retstat;
    off_t new_block_offset;

    // Free block exists, remove it from list
    if (free_blocks->head->next != free_blocks->head) {
        new_block_offset = (*((block_index_t *) free_blocks->head->next->data)) * BLOCK_SIZE;
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

int remove_block(const unsigned char hash[HASH_SIZE]) 
{
    hash_element_t *element = table_find(metadata, hash);
    if (element == NULL)
        return BLOCK_NOT_FOUND;
    
    block_index_t *block_index = malloc(sizeof(block_index_t *));
    *block_index = element->block_index;
    
    table_remove(&metadata, hash);
    list_add(free_blocks, block_index);

    if (check_and_defragment_blocks() < 0)
        return ERROR;

    return BLOCK_FOUND;
}

static ssize_t read_block(unsigned char buf[BLOCK_SIZE], off_t block_offset, ssize_t byte_count) {
    int retstat = log_syscall("pread", pread(blocks_fd, buf, byte_count, block_offset), 0);
    return retstat < 0 ? ERROR : retstat;
}

int add_reference_to_block(const unsigned char *buf, unsigned char hash[HASH_SIZE]) 
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

int remove_reference_from_block(const unsigned char hash[HASH_SIZE]) 
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

int copy_block_to_first_free(block_index_t src_index) {
    unsigned char src_block[BLOCK_SIZE];
    unsigned char src_hash[SHA_DIGEST_LENGTH];
    int retstat;

    retstat = read_block(src_block, src_index * BLOCK_SIZE, BLOCK_SIZE);
    if (retstat < 0) return ERROR;
    
    SHA1(src_block, BLOCK_SIZE, src_hash);
    block_index_t index = *((block_index_t *) free_blocks->head->next->data);
    
    retstat = create_block(src_block, src_hash);
    if (retstat < 0) return ERROR;
    
    hash_element_t *src_element = table_find(metadata, src_hash);
    src_element->block_index = index;
    
    return SUCCESS;
}

int check_and_defragment_blocks() {
    ssize_t blocks_filesize;
    block_index_t repo_blocks_count, free_blocks_count, repo_block_index;
    int retstat;

    blocks_filesize = get_virtual_file_size(blocks_fd);

    repo_blocks_count = blocks_filesize / BLOCK_SIZE;
    free_blocks_count = free_blocks->size;
    repo_block_index = repo_blocks_count - 1;

    // Compare the percentage of the real internal defragmentation with the max allowed
    if (free_blocks_count == 0 || ((float)free_blocks_count / repo_blocks_count) <= FRAGMENTATION_MAX_PERCENTAGE)
        return SUCCESS;

    // Above max allowance, defragment repository
    block_index_t blocks_to_defrag_count = ((free_blocks_count - FRAGMENTATION_MAX_PERCENTAGE * repo_blocks_count)
                                          / (1 - FRAGMENTATION_MAX_PERCENTAGE));

    // Moves the last used block to the first free block
    node_t *curr_node = free_blocks->head->prev;
    for (block_index_t i = 0; i < blocks_to_defrag_count; i++) {
        // If this block is free remove it from list and move both pointers
        if (*((block_index_t *)curr_node->data) == (repo_block_index - i)) { 
            curr_node = curr_node->prev;
            list_remove_element(free_blocks, curr_node->next);
        } else { // This block needs to be moved to the first free in the repository
            copy_block_to_first_free(repo_block_index - i);
        }
    }

    // Truncate blocks repository by the amount of blocks defragmented
    retstat = ftruncate(blocks_fd, blocks_filesize - blocks_to_defrag_count * BLOCK_SIZE);
    if (retstat < 0)
        return ERROR;

    return SUCCESS;
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
    block_offset_t last_block_size;
    ssize_t retstat = read_metadata_from_file(fd, (unsigned char *) &last_block_size);
    ssize_t virtual_file_size = get_virtual_file_size(fd);
    ssize_t size = last_block_size + ((virtual_file_size - VIRTFILE_METADATA_SIZE) / VIRTFILE_PTR_SIZE - (last_block_size != 0)) * BLOCK_SIZE;
    return retstat < 0 ? ERROR : MAX(size, 0);
}

ssize_t read_metadata_from_file(int fd, unsigned char buf[VIRTFILE_METADATA_SIZE]) {
    int retstat = log_syscall("pread", pread(fd, buf, VIRTFILE_METADATA_SIZE, 0), 0);
    return retstat < 0 ? ERROR : retstat;
}

ssize_t write_metadata_to_file(int fd, const unsigned char buf[VIRTFILE_METADATA_SIZE]) {
    int retstat = log_syscall("pwrite", pwrite(fd, buf, VIRTFILE_METADATA_SIZE, 0), 0);
    return retstat < 0 ? ERROR : retstat;
}

static ssize_t read_hash_from_file(int fd, unsigned char *hash, off_t block_offset, ssize_t count) {
    int retstat = log_syscall("pread", pread(fd, hash, VIRTFILE_PTR_SIZE * count, block_offset), 0);
    return retstat < 0 ? ERROR : retstat;
}

static ssize_t write_hash_to_file(int fd, unsigned char *hash, off_t block_offset, ssize_t count) {
    int retstat = log_syscall("pwrite", pwrite(fd, hash, VIRTFILE_PTR_SIZE * count, block_offset), 0);
    return retstat < 0 ? ERROR : retstat;
}

ssize_t read_block_from_file(int fd, char *buf, block_index_t block_index, block_offset_t block_offset, short byte_count) {
    int retstat;
    if (byte_count <= 0)
        return 0;

    unsigned char hash[HASH_SIZE];
    retstat = read_hash_from_file(fd, hash, VIRTFILE_METADATA_SIZE + block_index * VIRTFILE_PTR_SIZE, 1);
    if (retstat < 0) return ERROR;

    block_index_t index = table_find(metadata, hash)->block_index;
    retstat = read_block((unsigned char *) buf, index * BLOCK_SIZE + block_offset, MIN(byte_count, BLOCK_SIZE - block_offset));
    if (retstat < 0) return ERROR;

    return retstat;
}

ssize_t read_blocks_from_file(int fd, char *buf, block_index_t start_index, block_count_t block_count) {
    int retstat;
    if (block_count <= 0)
        return 0;
    ssize_t total_read = 0;
    off_t virtual_file_offset = VIRTFILE_METADATA_SIZE + start_index * VIRTFILE_PTR_SIZE;

    for (block_count_t i = 0; i < block_count; i++, virtual_file_offset += VIRTFILE_PTR_SIZE) {
        unsigned char hash[HASH_SIZE];
        retstat = read_hash_from_file(fd, hash, virtual_file_offset, 1);
        if (retstat < 0) return ERROR;

        block_index_t index = table_find(metadata, hash)->block_index;
        retstat = read_block((unsigned char *) buf + total_read, index * BLOCK_SIZE, BLOCK_SIZE);
        if (retstat < 0) return ERROR;
        else total_read += retstat;
    }

    return total_read;
}

ssize_t write_block_to_file(int fd, const char *buf, block_index_t block_index, block_offset_t block_offset, short byte_count) {
    int retstat;
    long long total_written = 0;
    if (byte_count <= 0)
        return 0;

    unsigned char new_block[BLOCK_SIZE];
    unsigned char old_hash[HASH_SIZE], new_hash[HASH_SIZE];
    off_t virtual_file_offset = VIRTFILE_METADATA_SIZE + block_index * VIRTFILE_PTR_SIZE;
    short old_block_exists;

    // Read current block contents in the current block_index 
    retstat = read_hash_from_file(fd, old_hash, virtual_file_offset, 1);
    if (retstat < 0) return ERROR;

    // Read previous block content or fill it with zeros
    old_block_exists = (retstat != 0);
    if (old_block_exists) {
        block_index_t index = table_find(metadata, old_hash)->block_index;
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
    if (memcmp(old_hash, new_hash, HASH_SIZE) == 0)
        return byte_count;

    retstat = write_hash_to_file(fd, new_hash, virtual_file_offset, 1);
    if (retstat < 0) return ERROR;

    return byte_count;
}

ssize_t write_blocks_to_file(int fd, const char *buf, block_index_t start_index, block_count_t block_count) {
    int retstat;
    if (block_count <= 0)
        return 0;
    off_t virtual_file_offset = VIRTFILE_METADATA_SIZE + start_index * VIRTFILE_PTR_SIZE;

    for (block_count_t i = 0; i < block_count; i++, virtual_file_offset += VIRTFILE_PTR_SIZE) {
        unsigned char old_hash[HASH_SIZE], new_hash[HASH_SIZE];
        retstat = read_hash_from_file(fd, old_hash, virtual_file_offset, 1);
        if (retstat < 0) return ERROR;
        
        retstat = add_reference_to_block((unsigned char *) buf, new_hash);
        if (retstat < 0) return ERROR;

        retstat = remove_reference_from_block(old_hash);
        if (retstat < 0) return ERROR;

        if (memcmp(old_hash, new_hash, HASH_SIZE) == 0)
            continue;

        retstat = write_hash_to_file(fd, new_hash, virtual_file_offset, 1);
        if (retstat < 0) return ERROR;
    }

    return block_count * BLOCK_SIZE;
}
 
int zeropad_file(int fd, ssize_t new_size) {
    int retstat = SUCCESS;
    ssize_t file_size = get_user_file_size(fd);
    if (file_size == ERROR) return ERROR;
    if (file_size >= new_size) return SUCCESS;

    unsigned char hash[HASH_SIZE];
    unsigned char buf[BLOCK_SIZE];

    block_index_t last_block_index = MAX((get_virtual_file_size(fd) - VIRTFILE_PTR_SIZE - VIRTFILE_METADATA_SIZE) / VIRTFILE_PTR_SIZE, 0);
    block_offset_t last_block_size;
    block_offset_t new_last_block_size = new_size % BLOCK_SIZE;

    // Read old last block size and update it
    retstat = read_metadata_from_file(fd, (unsigned char *) &last_block_size);
    if (retstat == ERROR) return ERROR;
    retstat = write_metadata_to_file(fd, (unsigned char *) &new_last_block_size);
    if (retstat == ERROR) return ERROR;
    
    // Fill the first block with zeros
    memset(buf, 0, BLOCK_SIZE);
    retstat = write_block_to_file(fd, (char *) buf, last_block_index, last_block_size, (BLOCK_SIZE - last_block_size) % BLOCK_SIZE);
    if (retstat == ERROR) return -1;

    // If not other zero blocks are needed
    long long bytes_to_pad = MAX(new_size - (file_size + BLOCK_SIZE - last_block_size), 0);
    block_count_t zero_blocks = bytes_to_pad / BLOCK_SIZE + (bytes_to_pad % BLOCK_SIZE != 0);
    if (zero_blocks == 0)
        return SUCCESS;

    // Create zero block
    memset(buf, 0, BLOCK_SIZE);
    retstat = add_reference_to_block(buf, hash);
    if (retstat == ERROR) return ERROR;

    // Write zero blocks to the file
    ssize_t offset = get_virtual_file_size(fd);
    for (block_count_t i = 0; i < zero_blocks; i++, offset += VIRTFILE_PTR_SIZE) {
        retstat = write_hash_to_file(fd, hash, offset, 1);
        if (retstat <= 0)
            break;
    }

    hash_element_t *zero_block_hash = table_find(metadata, hash);
    zero_block_hash->ref_count += zero_blocks - 1;

    return retstat < 0 ? ERROR : SUCCESS;
}

int truncate_file(int fd, ssize_t new_size) {
    int retstat = SUCCESS;
    ssize_t current_size = get_user_file_size(fd);
    if (current_size <= new_size)
        return zeropad_file(fd, new_size);

    block_offset_t last_block_size = new_size % BLOCK_SIZE;
    off_t new_real_size = VIRTFILE_METADATA_SIZE + (new_size / BLOCK_SIZE + (last_block_size != 0)) * VIRTFILE_PTR_SIZE;
    off_t offset = new_real_size;

    // Write new last_block_size
    retstat = write_metadata_to_file(fd, (unsigned char *) &last_block_size);
    if (retstat == ERROR) return ERROR;

    while (1) {
        unsigned char hash[HASH_SIZE];

        retstat = read_hash_from_file(fd, hash, offset, 1);
        if (retstat <= 0)
            break;

        remove_reference_from_block(hash);
        offset += VIRTFILE_PTR_SIZE;
    }
    if (retstat < 0) return ERROR;
    
    // Truncate real file to new size
    retstat = ftruncate(fd, new_real_size);

    return retstat < 0 ? ERROR : SUCCESS;
}