# fill_binary_blocks.py

def create_filled_binary_file(filename, max_value, block_size=4096):
    """
    Create a binary file where each byte value from 0 up to max_value (inclusive)
    is repeated for `block_size` bytes.

    :param filename: Name of the output binary file
    :param max_value: Highest byte value to write (0–255)
    :param block_size: Number of times to repeat each byte
    """
    if not (0 <= max_value <= 255):
        raise ValueError("max_value must be between 0 and 255")

    with open(filename, 'wb') as f:
        for value in range(max_value + 1):
            block = bytes([value]) * block_size
            f.write(block)

    print(f"Created '{filename}' with byte values 0 to {max_value}, each repeated {block_size} times.")

if __name__ == "__main__":
    # Example usage
    create_filled_binary_file("output.bin", max_value=10)
