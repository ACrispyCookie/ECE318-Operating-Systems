import os
import random
import string
import time
import sys
import filecmp

MOUNTPOINT = "../example/mountdir"  # Your mountpoint
BASENAME = "testfile"
LOCAL_DIR = os.path.dirname(os.path.abspath(__file__))

FS_FILE = os.path.join(MOUNTPOINT, BASENAME)
REF_FILE = os.path.join(LOCAL_DIR, f"{BASENAME}_ref")
RENAMED_FS_FILE = os.path.join(MOUNTPOINT, "renamed_testfile")
RENAMED_REF_FILE = os.path.join(LOCAL_DIR, "renamed_testfile_ref")

MAX_FILE_SIZE = 100 * 1024 * 1024  # 10 MB
MIN_FILE_SIZE = 10 * 1024 * 1024 # 1 MB
QUIET = "--quiet" in sys.argv

def log(msg):
    if not QUIET:
        print(msg)

def important(msg):
    print(msg)

def random_data(size):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=size)).encode()

def fail(message):
    print(f"[FAIL] {message}")
    sys.exit(1)

def files_equal(path1, path2):
    if not filecmp.cmp(path1, path2, shallow=False):
        fail(f"Files differ: {path1} vs {path2}")

def write_random_chunks(fs_path, ref_path, total_size):
    with open(fs_path, "wb") as f_fs, open(ref_path, "wb") as f_ref:
        written = 0
        while written < total_size:
            chunk_size = random.randint(1, 8192)
            chunk_size = min(chunk_size, total_size - written)
            data = random_data(chunk_size)
            f_fs.write(data)
            f_ref.write(data)
            written += chunk_size
            log(f"[WRITE] {chunk_size} bytes (total: {written}/{total_size})")

def read_random_chunks(path1, path2):
    with open(path1, "rb") as f1, open(path2, "rb") as f2:
        while True:
            size = random.randint(1, 8192)
            d1 = f1.read(size)
            d2 = f2.read(size)
            if d1 != d2:
                fail(f"[READ MISMATCH] Data differs in read from {path1} vs {path2}")
            if not d1:
                break
            log(f"[READ] {len(d1)} bytes matched")

def truncate_to_random_size(fs_path, ref_path):
    size = os.path.getsize(fs_path)
    if size == 0:
        return
    new_size = random.randint(0, size)
    with open(fs_path, "ab") as f_fs, open(ref_path, "ab") as f_ref:
        os.ftruncate(f_fs.fileno(), new_size)
        os.ftruncate(f_ref.fileno(), new_size)
    log(f"[TRUNCATE] Both files truncated to {new_size}")
    files_equal(fs_path, ref_path)

def overwrite_random(fs_path, ref_path):
    size = os.path.getsize(fs_path)
    if size == 0:
        return
    offset = random.randint(0, size - 1)
    length = random.randint(1, min(4096, size - offset))
    data = random_data(length)
    with open(fs_path, "r+b") as f_fs, open(ref_path, "r+b") as f_ref:
        f_fs.seek(offset)
        f_fs.write(data)
        f_ref.seek(offset)
        f_ref.write(data)
    log(f"[OVERWRITE] {length} bytes at offset {offset}")
    files_equal(fs_path, ref_path)

def rename_and_verify(old_fs, new_fs, old_ref, new_ref):
    os.rename(old_fs, new_fs)
    os.rename(old_ref, new_ref)
    log(f"[RENAME] Renamed both files")
    files_equal(new_fs, new_ref)

def unlink_and_verify(fs_path, ref_path):
    os.unlink(fs_path)
    os.unlink(ref_path)
    if os.path.exists(fs_path) or os.path.exists(ref_path):
        fail("Files still exist after unlink")
    log(f"[UNLINK] Deleted both files")

def run_tests():
    important("=== Filesystem Functional Test with Comparison ===")

    # Clean up
    for f in [FS_FILE, REF_FILE, RENAMED_FS_FILE, RENAMED_REF_FILE]:
        if os.path.exists(f):
            os.remove(f)

    # Write
    size = random.randint(MIN_FILE_SIZE, MAX_FILE_SIZE)
    important(f"[STEP] Writing {size} bytes")
    write_random_chunks(FS_FILE, REF_FILE, size)
    files_equal(FS_FILE, REF_FILE)

    # Read
    important("[STEP] Reading and comparing reads")
    read_random_chunks(FS_FILE, REF_FILE)

    # Truncate
    important("[STEP] Truncating randomly")
    for _ in range(10):
        truncate_to_random_size(FS_FILE, REF_FILE)
        time.sleep(0.05)

    # Overwrite
    important("[STEP] Overwriting random positions")
    for _ in range(100):
        overwrite_random(FS_FILE, REF_FILE)

    # Rename
    important("[STEP] Renaming both files")
    rename_and_verify(FS_FILE, RENAMED_FS_FILE, REF_FILE, RENAMED_REF_FILE)

    # Read again after rename
    important("[STEP] Reading after rename")
    read_random_chunks(RENAMED_FS_FILE, RENAMED_REF_FILE)

    # Delete
    important("[STEP] Deleting both files")
    unlink_and_verify(RENAMED_FS_FILE, RENAMED_REF_FILE)

    important("[SUCCESS] All operations succeeded and files matched.")

if __name__ == "__main__":
    run_tests()
