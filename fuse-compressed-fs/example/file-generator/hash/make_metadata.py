import sys
import hashlib
import struct

BLOCK_SIZE = 4096

def load_metadata(metadata_file):
    values = []
    with open(metadata_file, 'r') as f:
        for line_number, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue  # skip empty lines
            try:
                value = int(line)
            except ValueError:
                raise ValueError(f"Invalid integer on line {line_number}: '{line}'")
            if value < 0 or value > 0xFFFFFFFF:
                raise ValueError(f"Integer on line {line_number} is out of 32-bit unsigned range.")
            values.append(value)
    return values

def process_blocks(input_file, output_file, metadata_file):
    metadata_values = load_metadata(metadata_file)

    with open(input_file, 'rb') as f_in, open(output_file, 'wb') as f_out:
        index = 0
        while True:
            block = f_in.read(BLOCK_SIZE)
            if not block:
                break
            if len(block) < BLOCK_SIZE:
                raise ValueError(f"Block {index} is incomplete ({len(block)} bytes).")

            if index >= len(metadata_values):
                raise ValueError(f"Metadata file has fewer entries than blocks. Block {index} is missing.")

            sha1_hash = hashlib.sha1(block).digest()                  # 20 bytes
            value_int = struct.pack('<I', metadata_values[index])     # 4 bytes
            block_index = struct.pack('<I', index)                    # 4 bytes

            f_out.write(sha1_hash + value_int + block_index)
            index += 1

        if index != len(metadata_values):
            raise ValueError(f"Metadata file has {len(metadata_values)} entries but only {index} blocks were processed.")

    print(f"Processed {index} blocks into '{output_file}' using metadata from '{metadata_file}'.")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input_blocks.bin> <output_hash_index.bin> <metadata.txt>")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]
    metadata_path = sys.argv[3]

    process_blocks(input_path, output_path, metadata_path)
