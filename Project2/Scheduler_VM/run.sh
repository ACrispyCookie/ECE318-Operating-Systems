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

# Run the make command
make
if [ $? -ne 0 ]; then
    echo "Make command failed."
    exit 1
fi

# Create the outputs directory if it doesn't exist
OUTPUT_DIR="outputs"
mkdir -p $OUTPUT_DIR

# Run the executable with the input file and redirect output
EXECUTABLE="./sjf_sched"
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
