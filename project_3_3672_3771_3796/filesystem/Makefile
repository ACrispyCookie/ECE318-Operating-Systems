CC = gcc
CFLAGS = -g -Wall -D_FILE_OFFSET_BITS=64 $(shell pkg-config fuse --cflags) $(shell pkg-config --cflags libssl) -I.
LDFLAGS = $(shell pkg-config fuse --libs) $(shell pkg-config --libs libssl) -lssl -lcrypto

SRC = \
	log.c \
	nodes_hashtable/nodes_hashtable.c \
	blocks_hashtable/blocks_hashtable.c \
	list/list.c \
	fs_manager.c \
	bbfs.c

OBJ = $(SRC:.c=.o)

TARGET = bbfs

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
