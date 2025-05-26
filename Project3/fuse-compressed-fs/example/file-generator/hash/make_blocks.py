import sys

BLOCK_SIZE = 4096

def parse_blocks_from_text(text):
    blocks = []
    parts = text.split('Block:')
    for part in parts[1:]:  # Skip the first split (before the first 'Block:')
        content = part.strip()
        block_bytes = content.encode('utf-8')
        if len(block_bytes) > BLOCK_SIZE:
            block_bytes = block_bytes[:BLOCK_SIZE]
        else:
            block_bytes += b'\x00' * (BLOCK_SIZE - len(block_bytes))
        blocks.append(block_bytes)
    return blocks

def write_blocks_to_binary_file(blocks, output_file):
    with open(output_file, 'wb') as f:
        for block in blocks:
            f.write(block)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.txt> <output.bin>")
        sys.exit(1)

    input_text_path = sys.argv[1]
    output_bin_path = sys.argv[2]

    with open(input_text_path, 'r', encoding='utf-8') as f:
        text = f.read()

    blocks = parse_blocks_from_text(text)
    write_blocks_to_binary_file(blocks, output_bin_path)

    print(f"Wrote {len(blocks)} blocks ({len(blocks) * BLOCK_SIZE} bytes) to '{output_bin_path}'")
