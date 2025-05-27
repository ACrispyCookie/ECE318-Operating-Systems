#include "fs_manager.h"
#include "log.h"
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

static ssize_t get_metadata(int fd, char buf[VIRTFILE_METADATA_SIZE]) {
    int retstat = log_syscall("pread", pread(fd, buf, VIRTFILE_METADATA_SIZE, 0), 0);
    return retstat < 0 ? ERROR : retstat;
}

static ssize_t get_block_hash(int fd, char hash[SHA_DIGEST_LENGTH], int block_index) {
    int retstat = log_syscall("pread", pread(fd, hash, VIRTFILE_PTR_SIZE, VIRTFILE_METADATA_SIZE + block_index * VIRTFILE_PTR_SIZE), 0);
    return retstat < 0 ? ERROR : retstat;
}

static ssize_t get_block(int fd, char buf[BLOCK_SIZE], int block_index) {
    int retstat = log_syscall("pread", pread(fd, buf, BLOCK_SIZE, block_index * BLOCK_SIZE), 0);
    return retstat < 0 ? ERROR : retstat;
}

int create_block(char *buf, char new_hash[SHA_DIGEST_LENGTH]) 
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
    element_t *element = table_add(new_hash, 1, new_block_offset);

    return SUCCESS;
}

int remove_block(char hash[SHA_DIGEST_LENGTH]) 
{
    element_t *element = table_find(hash);
    if (element == NULL)
        return BLOCK_NOT_FOUND;
    
    unsigned int *block_offset = malloc(sizeof(unsigned int *));
    *block_offset = element->offset;
    
    table_remove(hash);
    list_add(free_blocks, block_offset);

    return BLOCK_FOUND;
}

int find_or_create_block(char *buf, char hash[SHA_DIGEST_LENGTH]) 
{
    SHA1(buf, BLOCK_SIZE, hash);

    element_t *element = table_find(hash);
    if (element == NULL) {
        int retstat = create_block(buf, hash);
        return retstat < 0 ? ERROR : BLOCK_CREATED;
    }
    
    element->ref_count++;

    return BLOCK_FOUND;
}

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

ssize_t read_file_blocks(int fd, unsigned int start_index, char *buf, int block_count) {
    int retstat;
    if (block_count < 0)
        return 0;
    int total_read = 0;

    for (int i = 0; i < block_count; i++) {
        unsigned char hash[SHA_DIGEST_LENGTH];
        retstat = get_block_hash(fd, hash, start_index + i);
        if (retstat < 0) return ERROR;

        int offset = table_find(hash);
        retstat = get_block(fd, buf + i * BLOCK_SIZE, offset);
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
    retstat = log_syscall("pwrite", pwrite(fd, &last_block_size, VIRTFILE_METADATA_SIZE, 0), 0);
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