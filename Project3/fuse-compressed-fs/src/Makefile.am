bin_PROGRAMS = bbfs
bbfs_SOURCES = bbfs.c log.c log.h params.h hashtable/uthash.h hashtable/hashtable.h hashtable/hashtable.c list/list.c list/list.h fs_manager.h fs_manager.c
AM_CFLAGS = @FUSE_CFLAGS@ -Wall
LDADD = @FUSE_LIBS@ -lssl -lcrypto
