import os
import io
import sys
import string
import random
import unittest
import shutil

BLOCK_SIZE = 4096

CWD = os.path.dirname(os.path.abspath(__file__))
MNT = os.path.abspath(os.path.join(CWD, "./fs/mountdir/"))
ROOT = os.path.abspath(os.path.join(CWD, "./fs/rootdir/"))

BBFS_EXECUTABLE_PATH = os.path.abspath(os.path.join(CWD, "../src/bbfs"))

BLOCKS_REPOSITORY_PATH = os.path.join(ROOT, "blocks")


def mount_bbfs():
    if not os.path.exists(MNT):
        os.makedirs(MNT)
    if not os.path.exists(ROOT):
        os.makedirs(ROOT)

    # Ensure the executable runs in the directory the tests are in to generate logs
    os.chdir(CWD)
    os.system(f"{BBFS_EXECUTABLE_PATH} {ROOT} {MNT} 2> /dev/null")


def unmount_bbfs():
    os.system(f"fusermount -u {MNT}")


def random_data(size: int) -> bytes:
    return ''.join(random.choices(string.ascii_letters + string.digits, k=size)).encode()


class TestBlocks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        mount_bbfs()

    @classmethod
    def tearDownClass(cls):
        unmount_bbfs()

    def setUp(self):
        self.f_path = os.path.join(MNT, "bogus")

    def tearDown(self):
        try:
            os.remove(self.f_path)
        except FileNotFoundError:
            pass

    def write_and_read(self, data, truncate_to=None, overwrite_at=None, overwrite_chunk=None):
        with open(self.f_path, "wb+") as f:
            f.write(data)
            if overwrite_at is not None and overwrite_chunk is not None:
                f.seek(overwrite_at)
                f.write(overwrite_chunk)
            if truncate_to is not None:
                os.ftruncate(f.fileno(), truncate_to)

        with open(self.f_path, "rb") as f:
            return f.read()

    def test_write_one_block(self):
        data = random_data(BLOCK_SIZE)
        read_back = self.write_and_read(data)
        self.assertEqual(read_back, data)

    def test_write_ten_blocks(self):
        data = random_data(BLOCK_SIZE * 10)
        read_back = self.write_and_read(data)
        self.assertEqual(read_back, data)

    def test_write_ten_and_a_half_blocks(self):
        data = random_data(int(BLOCK_SIZE * 10.5))
        read_back = self.write_and_read(data)
        self.assertEqual(read_back, data)

    def test_overwrite_blocks_data(self):
        data = random_data(int(BLOCK_SIZE * 5.5))
        chunk = random_data(int(BLOCK_SIZE / 10))
        offset = BLOCK_SIZE * 3
        read_back = self.write_and_read(data, overwrite_at=offset, overwrite_chunk=chunk)

        expected = bytearray(data)
        expected[offset:offset+len(chunk)] = chunk
        self.assertEqual(read_back, bytes(expected))

    def test_compression(self):
        # Write the same data to the same file twice
        data = random_data(int(BLOCK_SIZE * 10.5))
        file1_path = os.path.join(MNT, "file1")
        file2_path = os.path.join(MNT, "file2")

        with open(file1_path, "wb") as f1, open(file2_path, "wb") as f2:
            f1.write(data)
            f2.write(data)

        with open(file1_path, "rb") as f1, open(file2_path, "rb") as f2:
            self.assertEqual(f1.read(), f2.read())

        # Assert block-level deduplication
        expected_size = BLOCK_SIZE * 11
        actual_size = os.stat(BLOCKS_REPOSITORY_PATH).st_size

        self.assertEqual(actual_size, expected_size)

        os.remove(file1_path)
        os.remove(file2_path)

    def test_ftruncate_files(self):
        data = random_data(int(BLOCK_SIZE * 5.5))
        truncated = self.write_and_read(data, truncate_to=BLOCK_SIZE * 2)
        self.assertEqual(truncated, data[:BLOCK_SIZE * 2])

    def test_add_zero_padding_to_files_with_ftruncate(self):
        data = random_data(int(BLOCK_SIZE * 5.5))
        padded = self.write_and_read(data, truncate_to=BLOCK_SIZE * 10)

        expected = data + b'\x00' * (BLOCK_SIZE * 10 - len(data))
        self.assertEqual(padded, expected)

    def test_defragmentation(self):
        file1_path = os.path.join(MNT, "file1")
        file2_path = os.path.join(MNT, "file2")

        # Step 1: Write large data to file1
        data1 = random_data(int(BLOCK_SIZE * 10.5))
        with open(file1_path, "wb") as f:
            f.write(data1)

        # Step 2: Write smaller data to file2
        data2 = random_data(int(BLOCK_SIZE * 5.5))
        with open(file2_path, "wb") as f:
            f.write(data2)

        # Step 3: Truncate file1 to simulate fragmentation
        with open(file1_path, "ab") as f:
            os.ftruncate(f.fileno(), int(BLOCK_SIZE * 0.5))

        # Validate final contents
        with open(file1_path, "rb") as f1, open(file2_path, "rb") as f2:
            self.assertEqual(f1.read(), data1[:int(BLOCK_SIZE * 0.5)])
            self.assertEqual(f2.read(), data2)

        os.remove(file1_path)
        os.remove(file2_path)


class TestNodes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        mount_bbfs()

    @classmethod
    def tearDownClass(cls):
        unmount_bbfs()

    def test_create_and_remove_directory(self):
        # Create directory in the mount point
        dir_path = os.path.join(MNT, "test_directory")
        os.makedirs(dir_path, exist_ok=True)

        self.assertTrue(os.path.isdir(dir_path))

        # Clean up the directory
        shutil.rmtree(dir_path)
    
        self.assertFalse(os.path.isdir(dir_path))

    def test_create_multiple_directories(self):
        # Create multiple directories
        for i in range(5):
            dir_path = os.path.join(MNT, f"test_directory_{i}")
            os.makedirs(dir_path, exist_ok=True)
            self.assertTrue(os.path.isdir(dir_path))

        # Clean up the directories
        for i in range(5):
            dir_path = os.path.join(MNT, f"test_directory_{i}")
            shutil.rmtree(dir_path)
            self.assertFalse(os.path.isdir(dir_path))

    def test_create_and_remove_directory_and_file(self):
        # Create file in the directory
        dir_path = os.path.join(MNT, "test_directory")

        os.makedirs(dir_path, exist_ok=True)
        file_path = os.path.join(dir_path, "test_file.txt")

        with open(file_path, "w") as f:
            f.write("This is a test file.")

        self.assertTrue(os.path.isfile(file_path))

        # Clean up the directory
        shutil.rmtree(dir_path)

        self.assertFalse(os.path.isdir(dir_path))

    def test_create_multiple_files_in_directory(self):
        # Create multiple files in a directory
        dir_path = os.path.join(MNT, "test_directory")
        os.makedirs(dir_path, exist_ok=True)

        for i in range(5):
            file_path = os.path.join(dir_path, f"test_file_{i}.txt")
            with open(file_path, "w") as f:
                f.write(f"This is test file {i}.")

        # Verify files were created
        for i in range(5):
            self.assertTrue(os.path.isfile(os.path.join(dir_path, f"test_file_{i}.txt")))

        # Clean up the directory
        shutil.rmtree(dir_path)
        self.assertFalse(os.path.isdir(dir_path))

    def test_create_nested_directories_and_file(self):
        # Create nested directories and a file
        nested_dir_path = os.path.join(MNT, "parent_dir", "child_dir")
        os.makedirs(nested_dir_path, exist_ok=True)

        file_path = os.path.join(nested_dir_path, "test_file.txt")
        with open(file_path, "w") as f:
            f.write("This is a test file in a nested directory.")

        self.assertTrue(os.path.isfile(file_path))

        # Clean up the nested directories
        shutil.rmtree(os.path.join(MNT, "parent_dir"))

        self.assertFalse(os.path.isdir(nested_dir_path))

    def test_create_multiple_directories_and_files(self):
        # Create multiple directories and files
        for i in range(3):
            dir_path = os.path.join(MNT, f"test_directory_{i}")
            os.makedirs(dir_path, exist_ok=True)

            for j in range(2):
                file_path = os.path.join(dir_path, f"test_file_{j}.txt")
                with open(file_path, "w") as f:
                    f.write(f"This is test file {j} in directory {i}.")

            # Verify directories and files were created
            self.assertTrue(os.path.isdir(dir_path))
            for j in range(2):
                self.assertTrue(os.path.isfile(os.path.join(dir_path, f"test_file_{j}.txt")))

            # Clean up the directories
            shutil.rmtree(dir_path)
            self.assertFalse(os.path.isdir(dir_path))

if __name__ == "__main__":
    print("Starting filesystems tests\n")
    print(f"FS executable at: {BBFS_EXECUTABLE_PATH}")
    print(f"Mount directory at: {MNT}")
    print(f"Root directory at: {ROOT}")
    print()
    print("Log files will be generated in the directory:", CWD)
    print()

    unittest.main()
