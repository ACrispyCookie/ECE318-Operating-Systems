CC = gcc
CFLAGS = -Wall -g
AR = ar
ARFLAGS = rcs
LIBNAME = libhello_syscall_wrapper.a
OBJ = syscall_wrapper.o
TARGET = hello

all: $(TARGET)

$(OBJ): syscall_wrapper.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBNAME): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET): hello.c $(LIBNAME)
	$(CC) $(CFLAGS) $< -o $@ -lhello_syscall_wrapper -L./

clean:
	rm -f $(OBJ) $(LIBNAME) $(TARGET)
