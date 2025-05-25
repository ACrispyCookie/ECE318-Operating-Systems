import struct
import sys

def write_raw_block(input_path, output_path):
    with open(input_path, "r", encoding="utf-8") as infile, open(output_path, "wb") as outfile:
        lines = [line.strip() for line in infile if line.strip()]
        if len(lines) < 2:
            print("Error: Input must contain at least two lines (size + ids).")
            return

        try:
            size_field = int(lines[0])
            ids = [int(x.strip()) for x in lines[1].split(",") if x.strip()]

            # Write size (2 bytes)
            outfile.write(struct.pack("<H", size_field))

            # Write each ID (4 bytes)
            for block_id in ids:
                outfile.write(struct.pack("<I", block_id))

            print(f"Wrote {len(ids)} IDs with size field {size_field} to {output_path}")

        except Exception as e:
            print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python make_fixed_header_block.py input.txt output.bin")
    else:
        write_raw_block(sys.argv[1], sys.argv[2])
