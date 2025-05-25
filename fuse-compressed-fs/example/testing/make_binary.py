import struct
import sys

def write_blocks(input_path, output_path):
    with open(input_path, "r", encoding="utf-8") as infile, open(output_path, "wb") as outfile:
        block_id = None
        data_lines = []

        for line in infile:
            line = line.rstrip("\n")
            if line.startswith("BLOCK:"):
                # Write previous block if exists
                if block_id is not None:
                    write_block(outfile, block_id, data_lines)
                block_id = int(line.split(":")[1].strip())
                data_lines = []
            else:
                data_lines.append(line)

        # Write the last block
        if block_id is not None:
            write_block(outfile, block_id, data_lines)

def write_block(outfile, block_id, lines):
    data = "\n".join(lines).encode("utf-8")
    if len(data) > 4096:
        raise ValueError(f"Block ID {block_id} exceeds 4096 bytes (actual size: {len(data)})")
    padded_data = data.ljust(4096, b'\x00')
    outfile.write(struct.pack("<I", block_id))  # 4 bytes, little-endian unsigned int
    outfile.write(padded_data)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python make_binary.py input.txt output.bin")
    else:
        write_blocks(sys.argv[1], sys.argv[2])
