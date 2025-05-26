import sys
import struct

HASH_ENTRY_SIZE = 28  # 20 (SHA-1) + 4 (zero) + 4 (offset)
HASH_SIZE = 20

def read_hash_index_file(hash_index_path):
    hashes = []
    with open(hash_index_path, 'rb') as f:
        while True:
            entry = f.read(HASH_ENTRY_SIZE)
            if not entry:
                break
            if len(entry) != HASH_ENTRY_SIZE:
                raise ValueError("Corrupt hash index entry.")
            sha1_hash = entry[:HASH_SIZE]
            hashes.append(sha1_hash)
    return hashes

def parse_text_file(text_file_path):
    with open(text_file_path, 'r') as f:
        lines = f.readlines()
    if len(lines) < 2:
        raise ValueError("Text file must have at least two lines.")
    
    short_int_value = int(lines[0].strip())
    index_list = list(map(int, lines[1].strip().split(',')))
    return short_int_value, index_list

def write_output_binary(output_path, short_int_value, selected_hashes):
    with open(output_path, 'wb') as f:
        f.write(struct.pack('<H', short_int_value))  # 2-byte unsigned short (little endian)
        for h in selected_hashes:
            f.write(h)

def main(hash_index_path, text_path, output_path):
    all_hashes = read_hash_index_file(hash_index_path)
    short_int_value, index_list = parse_text_file(text_path)

    selected_hashes = []
    for idx in index_list:
        if idx < 0 or idx >= len(all_hashes):
            raise IndexError(f"Invalid index: {idx}")
        selected_hashes.append(all_hashes[idx])

    write_output_binary(output_path, short_int_value, selected_hashes)
    print(f"Wrote output to '{output_path}' with {len(selected_hashes)} hashes.")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <hash_index.bin> <input.txt> <output.bin>")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2], sys.argv[3])
