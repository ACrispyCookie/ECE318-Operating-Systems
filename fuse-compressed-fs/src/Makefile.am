bin_PROGRAMS = bbfs
bbfs_SOURCES = bbfs.c log.c log.h params.h hashtable/uthash.h hashtable/hashtable.h hashtable/hashtable.c list/list.c list/list.h
AM_CFLAGS = @FUSE_CFLAGS@
LDADD = @FUSE_LIBS@ -lssl -lcrypto
