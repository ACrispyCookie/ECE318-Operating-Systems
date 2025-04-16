#!/bin/bash

# Check if the confs directory exists
CONFS_DIR="confs"
if [ ! -d "$CONFS_DIR" ]; then
    echo "The directory '$CONFS_DIR' does not exist."
    exit 1
fi

# List all .conf files in the confs directory
CONF_FILES=("$CONFS_DIR"/*.conf)
if [ ${#CONF_FILES[@]} -eq 0 ]; then
    echo "No .conf files found in the '$CONFS_DIR' directory."
    exit 1
fi

echo "Available configuration files:"
for i in "${!CONF_FILES[@]}"; do
    echo "$((i + 1)). $(basename "${CONF_FILES[$i]}")"
done

# Prompt the user to select a file
read -p "Enter the number of the configuration file to use: " SELECTION
if ! [[ "$SELECTION" =~ ^[0-9]+$ ]] || [ "$SELECTION" -lt 1 ] || [ "$SELECTION" -gt "${#CONF_FILES[@]}" ]; then
    echo "Invalid selection."
    exit 1
fi

# Get the selected file
INPUT_FILE="${CONF_FILES[$((SELECTION - 1))]}"

# Prompt the user to enable or disable the goodness algorithm
read -p "Do you want to enable the goodness algorithm? (y/n) [default: y]: " GOODNESS_CHOICE
if [[ "$GOODNESS_CHOICE" =~ ^[Nn]$ ]]; then
    MAKE_TARGET="no-goodness"
else
    MAKE_TARGET="all"
fi

# Run the make command with the selected target
make -C src $MAKE_TARGET
if [ $? -ne 0 ]; then
    echo "Make command failed."
    exit 1
fi

# Create the outputs directory if it doesn't exist
OUTPUT_DIR="outputs"
mkdir -p $OUTPUT_DIR

# Prompt the user to choose between normal execution or debugging
read -p "Do you want to debug the program using gdb? (y/n) [default: n]: " DEBUG_CHOICE
if [[ "$DEBUG_CHOICE" =~ ^[Yy]$ ]]; then
    # Run the executable in gdb
    gdb --args ./src/sjf_sched "$INPUT_FILE"
else
    # Run the executable with the input file and redirect output
    EXECUTABLE="./src/sjf_sched"
    OUTPUT_FILE="$OUTPUT_DIR/$(basename "${INPUT_FILE%.conf}").out"

    $EXECUTABLE $INPUT_FILE > $OUTPUT_FILE
    if [ $? -ne 0 ]; then
        echo "Execution of $EXECUTABLE failed."
        exit 1
    fi

    # Run the plot.py script with the generated .out file
    PLOT_SCRIPT="plot.py"
    python3 $PLOT_SCRIPT $OUTPUT_FILE
    if [ $? -ne 0 ]; then
        echo "Execution of $PLOT_SCRIPT failed."
        exit 1
    fi

    echo "Process completed successfully. Output saved to $OUTPUT_FILE."
fi