/*
  Big Brother File System
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h
  
  The point of this FUSE filesystem is to provide an introduction to
  FUSE.  It was my first FUSE filesystem as I got to know the
  software; hopefully, the comments in this code will help people who
  follow later to get a gentler introduction.

  This might be called a no-op filesystem:  it doesn't impose
  filesystem semantics on top of any other existing structure.  It
  simply reports the requests that come in, and passes them to an
  underlying filesystem.  The information is saved in a logfile named
  bbfs.log, in the directory from which you run bbfs.
*/
// #include "config.h"
#include "params.h"
#include "fs_manager.h"
#include "blocks_hashtable/blocks_hashtable.h"
#include "list/list.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <openssl/sha.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"

/** Translate virtual node paths to real paths
 *
 * Get the path relative to the mount dir and return the real absolute path.
 * The real_path will contain the translation from the virtual name to the real
 * file in the rootdir that contains the related metadata.
 *
 * e.g.: Given the virtual path "/somedir/file.txt" the real path will be
 *       "/absolute/path/to/mountdir/[node_id]" where [node_id] is the id
 *       of the requested node. That file contains the metadata.
 */
static int bb_fullpath(char real_path[PATH_MAX], const char *virtual_path, nodes_hash_element_t **dir_hashtable, nodes_hash_element_t **file_hashtable)
{
    char file_name[PATH_MAX];
    safe_basename(virtual_path, file_name);
    char node_id_str[NAME_MAX + 2];
    nodes_hash_element_t *node_entry;
    nodes_hash_element_t *dir_table = get_dir_node_from_path(virtual_path);

    if (!strcmp("/", file_name)) {
        if (dir_hashtable != NULL) *dir_hashtable = root_node_metadata;
        if (file_hashtable != NULL) *file_hashtable = root_node_metadata;
        strncpy(real_path, BB_DATA->rootdir, PATH_MAX - 1);
        strncat(real_path, ROOT_PATH, PATH_MAX - 1);
        return SUCCESS;
    }

    if (dir_table == NULL)
        return ERROR;

    if (dir_hashtable != NULL) *dir_hashtable = dir_table;
    strncpy(real_path, BB_DATA->rootdir, PATH_MAX - 1);
    strncat(real_path, DATA_PATH, PATH_MAX - 1);

    node_entry = nodes_table_find(dir_table->hashmap, file_name);
    log_msg("virt %s filename %s node_entry %p\n", virtual_path, file_name, node_entry);
    if (node_entry == NULL)
        return ERROR;

    if (file_hashtable != NULL) *file_hashtable = node_entry;
    snprintf(node_id_str, NAME_MAX + 1, "/%lu", node_entry->id);
    strncat(real_path, node_id_str, PATH_MAX - 1); // ridiculously long paths will break here

    log_msg("    bb_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
        BB_DATA->rootdir, virtual_path, real_path);

    return SUCCESS;
}

//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int bb_getattr(const char *path, struct stat *statbuf)
{
    ssize_t retstat;
    int fd;
    char fpath[PATH_MAX];
    nodes_hash_element_t *element;
    
    log_msg("\nbb_getattr(path=\"%s\", statbuf=0x%08x)\n", path, statbuf);

    if (bb_fullpath(fpath, path, NULL, &element) < 0) {
        log_stat(statbuf);
        return -ENOENT;
    }

    log_syscall("lstat", lstat(fpath, statbuf), 0);
    log_stat(statbuf);

    // nodes_table_print(root_node_metadata);

    // If it is a directory
    if (element->is_dir) {
        statbuf->st_size = 4096;
        statbuf->st_mode = S_IFDIR | 0775; // Random permissions to make it work
        return SUCCESS;
    }

    retstat = log_syscall("open", fd = open(fpath, O_RDONLY), 0);
    if (retstat < 0) return ERROR;

    retstat = get_user_file_size(fd);
    if (retstat == ERROR)
        return ERROR;

    statbuf->st_size = retstat;

    retstat = log_syscall("close", close(fd), 0);
    if (retstat < 0)
        return ERROR;

    return SUCCESS;
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to bb_readlink()
// bb_readlink() code by Bernardo F Costa (thanks!)
int bb_readlink(const char *path, char *link, size_t size)
{
    int retstat;
    char fpath[PATH_MAX];
    
    log_msg("\nbb_readlink(path=\"%s\", link=\"%s\", size=%d)\n",
	  path, link, size);
    bb_fullpath(fpath, path, NULL, NULL);

    retstat = log_syscall("readlink", readlink(fpath, link, size - 1), 0);
    if (retstat >= 0) {
        link[retstat] = '\0';
        retstat = 0;
        log_msg("    link=\"%s\"\n", link);
    }
    
    return retstat;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int bb_mknod(const char *path, mode_t mode, dev_t dev)
{
    int fd;
    ssize_t retstat;
    char new_file_path[PATH_MAX];
    char filename[PATH_MAX];
    safe_basename(path, filename);

    nodes_hash_element_t* dir_table;
    nodes_hash_element_t* new_node;

    log_msg("\nbb_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n", path, mode, dev);

    // Get the hashtable of the directory the new node is about to be created in
    dir_table = get_dir_node_from_path(path);
    if (dir_table == NULL)
        return -ENOENT;

    // Add the new node in the directory's hashtable
    new_node = nodes_table_add_new(&(dir_table->hashmap), filename, false);
    if (new_node == NULL)
        return -EEXIST;

    // Create the new file and write 0 as its last block size
    snprintf(new_file_path, PATH_MAX, "%s/%s/%lu", BB_DATA->rootdir, DATA_PATH, new_node->id);
    retstat = log_syscall("open", fd = open(new_file_path, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
    if (retstat < 0)
        return ERROR;

    block_offset_t last_block_size = 0;
    retstat = write_metadata_to_file(fd, (unsigned char *) &last_block_size);
    if (retstat < 0)
        return ERROR;

    retstat = log_syscall("close", close(fd), 0);
    if (retstat < 0)
        return ERROR;

    return SUCCESS;
}

/** Create a directory */
int bb_mkdir(const char *path, mode_t mode)
{
    int fd;
    ssize_t retstat;
    char new_file_path[PATH_MAX];
    char filename[PATH_MAX];
    safe_basename(path, filename);

    nodes_hash_element_t* dir_table;
    nodes_hash_element_t* new_node;
    
    log_msg("\nbb_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);

    // Get the hashtable of the directory the new node is about to be created in
    dir_table = get_dir_node_from_path(path);
    if (dir_table == NULL)
        return -ENOENT;
    
    // Add the new node in the directory's hashtable
    new_node = nodes_table_add_new(&(dir_table->hashmap), filename, true);
    if (new_node == NULL)
        return -EEXIST;
    
    // Create the new file and write 0 as its last block size
    snprintf(new_file_path, PATH_MAX, "%s/%s/%lu", BB_DATA->rootdir, DATA_PATH, new_node->id);

    retstat = log_syscall("open", fd = open(new_file_path, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
    if (retstat < 0)
        return ERROR;
    
    retstat = log_syscall("close", close(fd), 0);
    if (retstat < 0)
        return ERROR;

    return SUCCESS;
}

/** Remove a file */
int bb_unlink(const char *path)
{
    char fpath[PATH_MAX];
    int retstat, fd;
    nodes_hash_element_t *file_table;
    
    log_msg("bb_unlink(path=\"%s\")\n",
	    path);
    bb_fullpath(fpath, path, NULL, &file_table);
    
    if (file_table == NULL)
        return -ENOENT;
    if (file_table->is_dir)
        return -EISDIR;

    retstat = log_syscall("open", fd = open(fpath, O_RDWR), 0);
    if (retstat < 0) return -1;

    retstat = truncate_file(fd, 0);
    if (retstat == ERROR) return -1;

    retstat = log_syscall("close", close(fd), 0);
    if (retstat < 0) return -1;
    
    retstat = remove_file_node(path);
    if (retstat < 0) return -1;

    retstat = log_syscall("unlink", unlink(fpath), 0);
    if (retstat < 0) return -1;
    
    return SUCCESS;
}

/** Remove a directory */
int bb_rmdir(const char *path)
{
    char fpath[PATH_MAX];
    int retstat;
    nodes_hash_element_t *file_table;
    
    log_msg("bb_rmdir(path=\"%s\")\n",
	    path);
    bb_fullpath(fpath, path, NULL, &file_table);
    
    if (file_table == NULL)
        return -ENOENT;
    if (!file_table->is_dir)
        return -ENOTDIR;
    if (file_table->hashmap != NULL)
        return -ENOTEMPTY;
    
    retstat = remove_file_node(path);
    if (retstat < 0) return -1;

    retstat = log_syscall("unlink", unlink(fpath), 0);
    if (retstat < 0) return -1;
    
    return SUCCESS;
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int bb_symlink(const char *path, const char *link)
{
    char flink[PATH_MAX];
    nodes_hash_element_t *file_table;
    
    log_msg("\nbb_symlink(path=\"%s\", link=\"%s\")\n",
	    path, link);
    bb_fullpath(flink, path, NULL, &file_table);
    
    if (file_table == NULL)
        return -ENOENT;

    return log_syscall("symlink", symlink(path, flink), 0);
}

/** Rename a file */
// both path and newpath are fs-relative
int bb_rename(const char *path, const char *newpath)
{
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    int retstat;
    nodes_hash_element_t *dir_entry = NULL;
    nodes_hash_element_t *file_entry = NULL;
    nodes_hash_element_t *new_dir_entry = NULL;
    nodes_hash_element_t *new_file_entry = NULL;
    
    log_msg("\nbb_rename(fpath=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    bb_fullpath(fpath, path, &dir_entry, &file_entry);
    bb_fullpath(fnewpath, newpath, &new_dir_entry, &new_file_entry);
    log_msg("   old path: %s new path: %s old_real_path: %s new_real_path: %s dir_entry: %p file_entry: %p new_dir_entry %p new_file_entry %p\n", path, newpath, fpath, fnewpath, 
        dir_entry, file_entry, new_dir_entry, new_file_entry);
    nodes_table_print(root_node_metadata->hashmap);
    log_msg("a-2\n");

    if (file_entry == new_file_entry && dir_entry == new_dir_entry)
        return SUCCESS;
    log_msg("a-1\n");
    
    if (file_entry == NULL || new_dir_entry == NULL)
        return -ENOENT;
    log_msg("a0\n");

    // File to be renamed is a dir and a regular file exists in the new path
    if (file_entry->is_dir && new_file_entry != NULL && !new_file_entry->is_dir)
        return -EEXIST;

    // Remove file to be renamed from the old directory
    log_msg("a1\n");
    retstat = nodes_table_remove_element(&(dir_entry->hashmap), file_entry);
    log_msg("a2 %d\n", retstat);
    if (retstat < 0)
        return ERROR;

    // File exists in the new path and is of the same type
    if (new_file_entry != NULL && new_file_entry->is_dir == file_entry->is_dir) {
        log_msg("a3\n");
        // Remove old file
        int fd;
        retstat = log_syscall("open", fd = open(fnewpath, O_RDWR), 0);
        if (retstat < 0) return -1;

        retstat = truncate_file(fd, 0);
        if (retstat == ERROR) return -1;

        retstat = log_syscall("close", close(fd), 0);
        if (retstat < 0) return -1;

        retstat = log_syscall("unlink", unlink(fnewpath), 0);
        if (retstat < 0) return -1;

        // Replace old file entry
        new_file_entry->id = file_entry->id;
        new_file_entry->hashmap = file_entry->hashmap;
        free(file_entry);

        return SUCCESS;
    }

    // Change name 
    char new_dir_name[PATH_MAX];
    log_msg("a44 %s\n", newpath);
    safe_basename(newpath, new_dir_name);
    strcpy(file_entry->name, new_dir_name);
    log_msg("a4 %s\n", new_dir_name);

    log_msg("a5\n");
    retstat = nodes_table_add_element(&(new_dir_entry->hashmap), file_entry);
    log_msg("a6 %d\n", retstat);
    if (retstat < 0)
        return ERROR;

    log_msg("a7\n");
    return SUCCESS;
}

/** Create a hard link to a file */
int bb_link(const char *path, const char *newpath)
{
    char fpath[PATH_MAX], fnewpath[PATH_MAX];
    
    log_msg("\nbb_link(path=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    bb_fullpath(fpath, path, NULL, NULL);
    bb_fullpath(fnewpath, newpath, NULL, NULL);

    return log_syscall("link", link(fpath, fnewpath), 0);
}

/** Change the permission bits of a file */
int bb_chmod(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_chmod(fpath=\"%s\", mode=0%03o)\n",
	    path, mode);
    bb_fullpath(fpath, path, NULL, NULL);

    return log_syscall("chmod", chmod(fpath, mode), 0);
}

/** Change the owner and group of a file */
int bb_chown(const char *path, uid_t uid, gid_t gid)
  
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_chown(path=\"%s\", uid=%d, gid=%d)\n",
	    path, uid, gid);
    bb_fullpath(fpath, path, NULL, NULL);

    return log_syscall("chown", chown(fpath, uid, gid), 0);
}

/** Change the size of a file */
int bb_truncate(const char *path, off_t newsize)
{
    char fpath[PATH_MAX];
    int retstat = 0;
    
    log_msg("\nbb_truncate(path=\"%s\", newsize=%lld)\n",
	    path, newsize);
    bb_fullpath(fpath, path, NULL, NULL);

    int new_fd = log_syscall("open", open(fpath, O_RDWR), 0);
    if (new_fd < 0) return -1;

    retstat = truncate_file(new_fd, newsize);
    if (retstat == ERROR) return -1;

    retstat = log_syscall("close", close(new_fd), 0);
    if (retstat < 0) return -1;

    return retstat;
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int bb_utime(const char *path, struct utimbuf *ubuf)
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_utime(path=\"%s\", ubuf=0x%08x)\n",
	    path, ubuf);
    bb_fullpath(fpath, path, NULL, NULL);

    return log_syscall("utime", utime(fpath, ubuf), 0);
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int bb_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    int fd;
    char fpath[PATH_MAX];
    
    log_msg("\nbb_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);
    bb_fullpath(fpath, path, NULL, NULL);
    
    // if the open call succeeds, my retstat is the file descriptor,
    // else it's -errno.  I'm making sure that in that case the saved
    // file descriptor is exactly -1.
    fd = log_syscall("open", open(fpath, fi->flags), 0);
    if (fd < 0)
	retstat = log_error("open");
	
    fi->fh = fd;

    log_fi(fi);
    
    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int bb_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{   
    block_count_t block_count; // Total blocks to read
    block_offset_t first_offset; // Offset in the first block
    block_index_t block_hash_position; // Position of the block hash inside the virtual file
    ssize_t total_read = 0; // Return status and total bytes read
    int retstat;

    log_msg("\nbb_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    first_offset = offset % BLOCK_SIZE;
    block_hash_position = offset / BLOCK_SIZE;
    block_count = CEIL_TO_MULT(first_offset + size, BLOCK_SIZE) / BLOCK_SIZE;

    // Read first block
    log_msg("read_path %s fd %d size %ld off %ld\n", path, fi->fh, size, offset);
    retstat = read_block_from_file(fi->fh, buf, block_hash_position, first_offset, MIN(BLOCK_SIZE - first_offset, size));
    if (retstat == ERROR) return -1;
    total_read += retstat;

    // Read middle blocks
    retstat = read_blocks_from_file(fi->fh, buf + total_read, block_hash_position + 1, block_count - 2);
    if (retstat == ERROR) return -1;
    total_read += retstat;

    // Read last block
    retstat = read_block_from_file(fi->fh, buf + total_read, block_hash_position + block_count - 1, 0, size - total_read);
    if (retstat == ERROR) return -1;
    total_read += retstat;

    return total_read;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int bb_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    char fpath[PATH_MAX];
    block_count_t block_count; // Total blocks to read
    block_offset_t first_offset; // Offset in the first block
    ssize_t  file_size; // File size in bytes
    block_index_t  block_hash_position; // Position of the block hash inside the virtual file
    block_offset_t last_block_size, new_last_block_size; // New last block size
    ssize_t total_write = 0; // Return status and total bytes written
    int retstat;

    log_msg("\nbb_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
    log_fi(fi);
    bb_fullpath(fpath, path, NULL, NULL);

    int new_fd = log_syscall("open", open(fpath, O_RDWR), 0);
    if (new_fd < 0) return -1;

    first_offset = offset % BLOCK_SIZE;
    block_hash_position = offset / BLOCK_SIZE;
    block_count = CEIL_TO_MULT(first_offset + size, BLOCK_SIZE) / BLOCK_SIZE;
    file_size = get_user_file_size(new_fd);

    // Zero pad file
    if (file_size > 0) {
        retstat = zeropad_file(new_fd, offset + size);
        if(retstat == ERROR) return -1;
    } else { // Or write new size if needed
        last_block_size = file_size % BLOCK_SIZE;
        new_last_block_size = (offset + size) % BLOCK_SIZE;

        if (new_last_block_size != last_block_size) {
            retstat = write_metadata_to_file(new_fd, (unsigned char *) &new_last_block_size);
            if(retstat == ERROR) return -1;
        }
    }

    // Write first block
    retstat = write_block_to_file(new_fd, buf, block_hash_position, first_offset, MIN(BLOCK_SIZE - first_offset, size));
    if (retstat == ERROR) return -1;
    total_write += retstat;

    // Write middle blocks
    retstat = write_blocks_to_file(new_fd, buf + total_write, block_hash_position + 1, block_count - 2);
    if (retstat == ERROR) return -1;
    total_write += retstat;

    // Write last block
    retstat = write_block_to_file(new_fd, buf + total_write, block_hash_position + block_count - 1, 0, size - total_write);
    if (retstat == ERROR) return -1;
    total_write += retstat;

    retstat = log_syscall("close", close(new_fd), 0);
    if (retstat < 0) return -1;

    return total_write;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int bb_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nbb_statfs(path=\"%s\", statv=0x%08x)\n",
	    path, statv);
    bb_fullpath(fpath, path, NULL, NULL);
    
    // get stats for underlying filesystem
    retstat = log_syscall("statvfs", statvfs(fpath, statv), 0);
    
    log_statvfs(statv);
    
    return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
// this is a no-op in BBFS.  It just logs the call and returns success
int bb_flush(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nbb_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);
	
    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int bb_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nbb_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    log_fi(fi);

    // We need to close the file.  Had we allocated any resources
    // (buffers etc) we'd need to free them here as well.
    return log_syscall("close", close(fi->fh), 0);
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int bb_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    log_msg("\nbb_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
    
    // some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
    if (datasync)
	return log_syscall("fdatasync", fdatasync(fi->fh), 0);
    else
#endif	
	return log_syscall("fsync", fsync(fi->fh), 0);
}

#ifdef HAVE_SYS_XATTR_H
/** Note that my implementations of the various xattr functions use
    the 'l-' versions of the functions (eg bb_setxattr() calls
    lsetxattr() not setxattr(), etc).  This is because it appears any
    symbolic links are resolved before the actual call takes place, so
    I only need to use the system-provided calls that don't follow
    them */

/** Set extended attributes */
int bb_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
	    path, name, value, size, flags);
    bb_fullpath(fpath, path, NULL, NULL);

    return log_syscall("lsetxattr", lsetxattr(fpath, name, value, size, flags), 0);
}

/** Get extended attributes */
int bb_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nbb_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
	    path, name, value, size);
    bb_fullpath(fpath, path, NULL, NULL);

    retstat = log_syscall("lgetxattr", lgetxattr(fpath, name, value, size), 0);
    if (retstat >= 0)
	log_msg("    value = \"%s\"\n", value);
    
    return retstat;
}

/** List extended attributes */
int bb_listxattr(const char *path, char *list, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    char *ptr;
    
    log_msg("\nbb_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
	    path, list, size
	    );
    bb_fullpath(fpath, path, NULL, NULL);

    retstat = log_syscall("llistxattr", llistxattr(fpath, list, size), 0);
    if (retstat >= 0) {
	log_msg("    returned attributes (length %d):\n", retstat);
	if (list != NULL)
	    for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
		log_msg("    \"%s\"\n", ptr);
	else
	    log_msg("    (null)\n");
    }
    
    return retstat;
}

/** Remove extended attributes */
int bb_removexattr(const char *path, const char *name)
{
    char fpath[PATH_MAX];
    
    log_msg("\nbb_removexattr(path=\"%s\", name=\"%s\")\n",
	    path, name);
    bb_fullpath(fpath, path, NULL, NULL);

    return log_syscall("lremovexattr", lremovexattr(fpath, name), 0);
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int bb_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    nodes_hash_element_t *element;
    
    log_msg("\nbb_opendir(path=\"%s\", fi=0x%08x)\n", path, fi);
    if (bb_fullpath(fpath, path, NULL, &element) < 0)
        return ERROR;

    // since opendir returns a pointer, takes some custom handling of
    // return status.
    fi->fh = (uint64_t) element->hashmap;
    
    log_fi(fi);
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

int bb_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;
    nodes_hash_element_t *dp;
    
    log_msg("\nbb_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
	    path, buf, filler, offset, fi);
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    dp = (nodes_hash_element_t *) fi->fh;

    // This will copy the entire directory into the buffer.  The loop exits
    // when either the system readdir() returns NULL, or filler()
    // returns something non-zero.  The first case just means I've
    // read the whole directory; the second means the buffer is full.
    filler(buf, "..", NULL, 0);
    for (nodes_hash_element_t *file = dp; file != NULL; file = file->hh.next) {
        log_msg("calling filler with name %s\n", file->name);
        if (filler(buf, file->name, NULL, 0) != 0) {
            log_msg("    ERROR bb_readdir filler:  buffer full");
            return -ENOMEM;
        }
    }
    filler(buf, ".", NULL, 0);
    
    log_fi(fi);
    
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int bb_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nbb_releasedir(path=\"%s\", fi=0x%08x)\n",
	    path, fi);
    log_fi(fi);
    fi->fh = 0;
    
    return retstat;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? >>> I need to implement this...
int bb_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nbb_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
    
    return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *bb_init(struct fuse_conn_info *conn)
{
    int retstat;
    log_msg("\nbb_init()\n");
    
    log_conn(conn);
    log_fuse_context(fuse_get_context());

    // Open blocks and metadata
    char blocks_path[PATH_MAX];
    char free_blocks_path[PATH_MAX];
    char blocks_metadata_path[PATH_MAX];
    char root_node_metadata_path[PATH_MAX];
    char user_folder_path[PATH_MAX];

    strcpy(blocks_path, BB_DATA->rootdir);
    strcat(blocks_path, BLOCKS_PATH);
    log_syscall("open", blocks_fd = open(blocks_path, O_CREAT | O_RDWR, STORAGE_FILES_PERMISSIONS), 0);

    strcpy(free_blocks_path, BB_DATA->rootdir);
    strcat(free_blocks_path, FREE_BLOCKS_PATH);
    log_syscall("open", free_blocks_fd = open(free_blocks_path, O_CREAT | O_RDWR, STORAGE_FILES_PERMISSIONS), 0);

    strcpy(blocks_metadata_path, BB_DATA->rootdir);
    strcat(blocks_metadata_path, BLOCKS_METADATA_PATH);
    log_syscall("open", blocks_metadata_fd = open(blocks_metadata_path, O_CREAT | O_RDWR, STORAGE_FILES_PERMISSIONS), 0);

    strcpy(user_folder_path, BB_DATA->rootdir);
    strcat(user_folder_path, DATA_PATH);
    log_syscall("mkdir", retstat = mkdir(user_folder_path, USER_FOLDER_PERMISSIONS), 0);
    if (retstat == -1 && errno != EEXIST) return NULL;

    strcpy(root_node_metadata_path, BB_DATA->rootdir);
    strcat(root_node_metadata_path, ROOT_PATH);
    log_syscall("open", root_node_metadata_fd = open(root_node_metadata_path, O_CREAT | O_RDWR, ROOT_FOLDER_PERMISSIONS), 0);

    // Load metadata and free blocks to memory
    load_blocks_metadata();
    load_node_metadata();
    load_free_blocks();
    
    return BB_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void bb_destroy(void *userdata)
{
    // Save metadata and free blocks and close files
    log_msg("\nbb_destroy(userdata=0x%08x)\n", userdata);

    save_blocks_metadata();
    save_node_metadata();
    save_free_blocks();
    close(blocks_fd);
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int bb_access(const char *path, int mask)
{
    int retstat = 0;
    char fpath[PATH_MAX];
   
    log_msg("\nbb_access(path=\"%s\", mask=0%o)\n",
	    path, mask);
    bb_fullpath(fpath, path, NULL, NULL);
    
    retstat = access(fpath, mask);
    
    if (retstat < 0)
	retstat = log_error("bb_access access");
    
    return retstat;
}

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int bb_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    char fpath[PATH_MAX];
    int retstat = 0;
    log_msg("\nbb_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n",
	    path, offset, fi);
    log_fi(fi);
    bb_fullpath(fpath, path, NULL, NULL);

    int new_fd = log_syscall("open", open(fpath, O_RDWR), 0);
    if (new_fd < 0) return -1;

    retstat = truncate_file(new_fd, offset);
    if (retstat == ERROR) return -1;

    retstat = log_syscall("close", close(new_fd), 0);
    if (retstat < 0) return -1;
    
    return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int bb_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0, fd;
    char fpath[PATH_MAX];
    log_msg("\nbb_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n",
	    path, statbuf, fi);
    log_fi(fi);
    nodes_hash_element_t *element;
    if (bb_fullpath(fpath, path, NULL, &element) < 0)
        return ERROR;

    // On FreeBSD, trying to do anything with the mountpoint ends up
    // opening it, and then using the FD for an fgetattr.  So in the
    // special case of a path of "/", I need to do a getattr on the
    // underlying root directory instead of doing the fgetattr().
    if (!strcmp(path, "/"))
	return bb_getattr(path, statbuf);
    
    retstat = fstat(fi->fh, statbuf);
    if (retstat < 0)
	retstat = log_error("bb_fgetattr fstat");

    if (element->is_dir) {
        statbuf->st_size = 4096;
        return SUCCESS;
    }

    retstat = log_syscall("open", fd = open(fpath, O_RDONLY), 0);
    if (retstat < 0) return ERROR;

    retstat = get_user_file_size(fd);
    if (retstat == ERROR)
        return ERROR;

    statbuf->st_size = retstat;

    retstat = log_syscall("close", close(fd), 0);
    if (retstat < 0)
        return ERROR;
    
    log_stat(statbuf);
    
    return retstat;
}

struct fuse_operations bb_oper = {
  .getattr = bb_getattr,
  .readlink = bb_readlink,
  // no .getdir -- that's deprecated
  .getdir = NULL,
  .mknod = bb_mknod,
  .mkdir = bb_mkdir,
  .unlink = bb_unlink,
  .rmdir = bb_rmdir,
  .symlink = bb_symlink,
  .rename = bb_rename,
  .link = bb_link,
  .chmod = bb_chmod,
  .chown = bb_chown,
  .truncate = bb_truncate,
  .utime = bb_utime,
  .open = bb_open,
  .read = bb_read,
  .write = bb_write,
  /** Just a placeholder, don't set */ // huh???
  .statfs = bb_statfs,
  .flush = bb_flush,
  .release = bb_release,
  .fsync = bb_fsync,
  
#ifdef HAVE_SYS_XATTR_H
  .setxattr = bb_setxattr,
  .getxattr = bb_getxattr,
  .listxattr = bb_listxattr,
  .removexattr = bb_removexattr,
#endif
  
  .opendir = bb_opendir,
  .readdir = bb_readdir,
  .releasedir = bb_releasedir,
  .fsyncdir = bb_fsyncdir,
  .init = bb_init,
  .destroy = bb_destroy,
  .access = bb_access,
  .ftruncate = bb_ftruncate,
  .fgetattr = bb_fgetattr
};

void bb_usage()
{
    fprintf(stderr, "usage:  bbfs [FUSE and mount options] rootDir mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct bb_state *bb_data;

    // bbfs doesn't do any access checking on its own (the comment
    // blocks in fuse.h mention some of the functions that need
    // accesses checked -- but note there are other functions, like
    // chown(), that also need checking!).  Since running bbfs as root
    // will therefore open Metrodome-sized holes in the system
    // security, we'll check if root is trying to mount the filesystem
    // and refuse if it is.  The somewhat smaller hole of an ordinary
    // user doing it with the allow_other flag is still there because
    // I don't want to parse the options string.
    if ((getuid() == 0) || (geteuid() == 0)) {
    	fprintf(stderr, "Running BBFS as root opens unnacceptable security holes\n");
    	return 1;
    }

    // See which version of fuse we're running
    fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
    
    // Perform some sanity checking on the command line:  make sure
    // there are enough arguments, and that neither of the last two
    // start with a hyphen (this will break if you actually have a
    // rootpoint or mountpoint whose name starts with a hyphen, but so
    // will a zillion other programs)
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	bb_usage();

    bb_data = malloc(sizeof(struct bb_state));
    if (bb_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the rootdir out of the argument list and save it in my
    // internal data
    bb_data->rootdir = realpath(argv[argc-2], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    bb_data->logfile = log_open();

    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &bb_oper, bb_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

    return fuse_stat;
}
