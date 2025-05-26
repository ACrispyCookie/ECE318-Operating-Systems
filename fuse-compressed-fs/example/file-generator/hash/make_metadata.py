import sys
import hashlib
import struct

BLOCK_SIZE = 4096

def process_blocks(input_file, output_file):
    with open(input_file, 'rb') as f_in, open(output_file, 'wb') as f_out:
        index = 0
        while True:
            block = f_in.read(BLOCK_SIZE)
            if not block:
                break
            if len(block) < BLOCK_SIZE:
                raise ValueError(f"Block {index} is incomplete ({len(block)} bytes).")

            sha1_hash = hashlib.sha1(block).digest()  # 20 bytes
            zero_int = struct.pack('<I', 0)            # 4 bytes
            block_index = struct.pack('<I', index)     # 4 bytes

            f_out.write(sha1_hash + zero_int + block_index)
            index += 1

    print(f"Processed {index} blocks into '{output_file}'.")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_blocks.bin> <output_hash_index.bin>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    process_blocks(input_path, output_path)
