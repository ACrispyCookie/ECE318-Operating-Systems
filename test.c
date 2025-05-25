#include <stdio.h>

#define CEIL_TO_MULT(x, n)  (((x) + (n - 1)) & ~(n - 1))
#define NUM_TESTS 4

int find_blocks(int offset, int size) {
	return CEIL_TO_MULT(offset % 4096 + size, 4096) / 4096;
}

int main(int argc, char *argv[]) {
	int voffset[NUM_TESTS] = {4095, 4096, 4096, 4095};
        int vsize[NUM_TESTS] = {4100, 4100, 8192, 8194};
	int vblocks[NUM_TESTS] = {3, 2, 2, 4};

	for (int i = 0; i < NUM_TESTS; i++) {
		int blocks = find_blocks(voffset[i], vsize[i]);
		if (blocks != vblocks[i]) {
			printf("error on %d got: %d expected: %d\n", i, blocks, vblocks[i]);
		}
	}


	return 0;
}
