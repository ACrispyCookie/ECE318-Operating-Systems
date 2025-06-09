import os
import sys
import string
import random
import unittest

BLOCK_SIZE = 4096

CWD = os.path.dirname(os.path.abspath(__file__))
MNT = os.path.abspath(os.path.join(CWD, "../example/mountdir/"))
ROOT = os.path.abspath(os.path.join(CWD, "../example/rootdir/"))

BLOCKS_REPOSITORY_PATH = os.path.join(ROOT, "blocks")


def random_data(size: int) -> bytes:
    return ''.join(random.choices(string.ascii_letters + string.digits, k=size)).encode()


class TestBlocks(unittest.TestCase):
    def setUp(self):
        # Create and open test and original files
        self.cwd_f_path = os.path.join(CWD, "bogus")
        self.mnt_f_path = os.path.join(MNT, "bogus")

        self.cwd_f = open(self.cwd_f_path, "wb")
        self.mnt_f = open(self.mnt_f_path, "wb")

    def tearDown(self):
        # Close the files if still open
        if not self.cwd_f.closed:
            self.cwd_f.close()

        if not self.mnt_f.closed:
            self.mnt_f.close()

        # Cleanup the files
        try:
            os.remove(self.cwd_f_path)
        except FileNotFoundError:
            pass

        try:
            os.remove(self.mnt_f_path)
        except FileNotFoundError:
            pass

    def test_write_one_block(self):
        # Write one block on both files
        data = random_data(BLOCK_SIZE)

        self.cwd_f.write(data)
        self.cwd_f.close()

        self.mnt_f.write(data)
        self.mnt_f.close()

        with open(self.cwd_f_path, "rb") as cwd_f, open(self.mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

    def test_write_ten_blocks(self):
        # Write ten blocks on both files
        data = random_data(BLOCK_SIZE * 10)

        self.cwd_f.write(data)
        self.cwd_f.close()

        self.mnt_f.write(data)
        self.mnt_f.close()

        with open(self.cwd_f_path, "rb") as cwd_f, open(self.mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

    def test_write_ten_and_a_half_blocks(self):
        # Write ten and a half blocks on both files
        data = random_data(int(BLOCK_SIZE * 10.5))

        self.cwd_f.write(data)
        self.cwd_f.close()

        self.mnt_f.write(data)
        self.mnt_f.close()

        with open(self.cwd_f_path, "rb") as cwd_f, open(self.mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

    def test_overwrite_blocks_data(self):
        # Write five and a half blocks on both files and write
        # a random chunk that is one tenth of a block
        data = random_data(int(BLOCK_SIZE * 5.5))
        chunk = random_data(int(BLOCK_SIZE / 10))

        self.cwd_f.write(data)
        self.cwd_f.seek(BLOCK_SIZE * 3)
        self.cwd_f.write(chunk)
        self.cwd_f.close()

        self.mnt_f.write(data)
        self.mnt_f.seek(BLOCK_SIZE * 3)
        self.mnt_f.write(chunk)
        self.mnt_f.close()

        with open(self.cwd_f_path, "rb") as cwd_f, open(self.mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

    def test_compression(self):
        # Write ten and a half blocks on first set of files
        data = random_data(int(BLOCK_SIZE * 10.5))

        self.cwd_f.write(data)
        self.cwd_f.close()

        self.mnt_f.write(data)
        self.mnt_f.close()

        # Write the same data on the second set of files
        cwd_f_path = os.path.join(CWD, "first_file.txt")
        mnt_f_path = os.path.join(MNT, "first_file.txt")

        cwd_f = open(cwd_f_path, "wb")
        cwd_f.write(data)
        cwd_f.close()

        mnt_f = open(mnt_f_path, "wb")
        mnt_f.write(data)
        mnt_f.close()

        with open(cwd_f_path, "rb") as cwd_f, open(mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

        # Assert the size of the blocks repository file being
        # only ten and a half blocks
        self.assertEqual(os.stat(BLOCKS_REPOSITORY_PATH).st_size, BLOCK_SIZE * 11)

    def test_ftruncate_files(self):
        # Write five and a half blocks on both files
        data = random_data(int(BLOCK_SIZE * 5.5))

        # Truncate both files to two blocks
        self.cwd_f.write(data)
        os.ftruncate(self.cwd_f.fileno(), int(BLOCK_SIZE * 2))
        self.cwd_f.close()

        self.mnt_f.write(data)
        os.ftruncate(self.mnt_f.fileno(), int(BLOCK_SIZE * 2))
        self.mnt_f.close()

        with open(self.cwd_f_path, "rb") as cwd_f, open(self.mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

    def test_add_zero_padding_to_files_with_ftruncate(self):
        # Write ten and a five blocks on both files
        data = random_data(int(BLOCK_SIZE * 5.5))

        # Add padding on both files until reaching ten blocks with ftruncate
        self.cwd_f.write(data)
        os.ftruncate(self.cwd_f.fileno(), BLOCK_SIZE * 10)
        self.cwd_f.close()

        self.mnt_f.write(data)
        os.ftruncate(self.mnt_f.fileno(), BLOCK_SIZE * 10)
        self.mnt_f.close()

        with open(self.cwd_f_path, "rb") as cwd_f, open(self.mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

    def test_defragmentation(self):
        # Write ten and a half blocks on first set of files
        data = random_data(int(BLOCK_SIZE * 10.5))

        self.cwd_f.write(data)
        self.cwd_f.close()

        self.mnt_f.write(data)
        self.mnt_f.close()

        with open(self.cwd_f_path, "rb") as cwd_f, open(self.mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

        # Write five and a half blocks on second set of files
        data = random_data(int(BLOCK_SIZE * 5.5))

        cwd_f_path = os.path.join(CWD, "first_file.txt")
        mnt_f_path = os.path.join(MNT, "first_file.txt")

        cwd_f = open(cwd_f_path, "wb")
        cwd_f.write(data)
        cwd_f.close()

        mnt_f = open(mnt_f_path, "wb")
        mnt_f.write(data)
        mnt_f.close()

        with open(cwd_f_path, "rb") as cwd_f, open(mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

        # Truncate the first set of files to half a block to create fragmentation
        # in the blocks repository and trigger defragmentation
        self.cwd_f = open(self.cwd_f_path, "ab")
        self.mnt_f = open(self.mnt_f_path, "ab")

        os.ftruncate(self.cwd_f.fileno(), int(BLOCK_SIZE * 0.5))
        self.cwd_f.close()

        os.ftruncate(self.mnt_f.fileno(), int(BLOCK_SIZE * 0.5))
        self.mnt_f.close()

        # Assert first set of files are equal
        with open(self.cwd_f_path, "rb") as cwd_f, open(self.mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

        # Assert second set of files are equal
        with open(cwd_f_path, "rb") as cwd_f, open(mnt_f_path, "rb") as mnt_f:
            cwd_data = cwd_f.read()
            mnt_data = mnt_f.read()

        self.assertEqual(cwd_data, mnt_data)

        # Cleanup first set of files
        os.remove(cwd_f_path)
        os.remove(mnt_f_path)

if __name__ == "__main__":
    unittest.main()
