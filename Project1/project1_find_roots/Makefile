LIBROOTS = libroots.a
ROOTS_OBJ = roots.o
ROOTS_SRC = roots.c
ROOTS_HEADER = roots.h
ROOTS_LIB_BIN = find_roots_lib
ROOTS_LIB_SRC = find_roots_lib.c 

all: $(LIBROOTS) $(ROOTS_LIB_BIN)

clean:
	rm -f $(ROOTS_LIB_BIN) $(LIBROOTS) $(ROOTS_OBJ)

$(ROOTS_LIB_BIN): $(ROOTS_LIB_SRC)
	gcc -Wall -g $(ROOTS_LIB_SRC) -o $(ROOTS_LIB_BIN) -lroots -L./

$(LIBROOTS): $(ROOTS_OBJ)
	ar rcs $(LIBROOTS) $(ROOTS_OBJ)

$(ROOTS_OBJ): $(ROOTS_SRC) $(ROOTS_HEADER)
	gcc -Wall -g -c $(ROOTS_SRC) -o $(ROOTS_OBJ)
