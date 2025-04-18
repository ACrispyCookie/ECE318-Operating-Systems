#!/bin/bash

# =======================================================================================
# CONFIGURATION FILE
# Check if --config=filename option is passed
INPUT_FILE=""
for arg in "$@"; do
    if [[ "$arg" == --config=* ]]; then
        INPUT_FILE="${arg#--config=}"
        break
    fi
done

# If no --config option is passed, list and prompt for selection
if [ -z "$INPUT_FILE" ]; then
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
fi

# Validate the input file
if [ ! -f "$INPUT_FILE" ]; then
    echo "The file '$INPUT_FILE' does not exist."
    exit 1
fi


# =======================================================================================
# GOODNESS ALGORITHM OPTION
# Check if --no-goodness option is passed
NO_GOODNESS=false
for arg in "$@"; do
    if [ "$arg" == "--no-goodness" ]; then
        NO_GOODNESS=true
        break
    fi
done

# Set the make target based on the presence of --no-goodness
if [ "$NO_GOODNESS" = true ]; then
    MAKE_TARGET="no-goodness"
else
    # Prompt the user to enable or disable the goodness algorithm
    echo ""
    read -p "Do you want to enable the goodness algorithm? (y/n) [default: y]: " GOODNESS_CHOICE
    if [[ "$GOODNESS_CHOICE" =~ ^[Nn]$ ]]; then
        MAKE_TARGET="no-goodness"
    else
        MAKE_TARGET="all"
    fi
fi

# =======================================================================================
# RUN MAKE
# Run the make command with the selected target
make -C src $MAKE_TARGET
if [ $? -ne 0 ]; then
    echo "Make command failed."
    exit 1
fi

# Create the outputs directory if it doesn't exist
OUTPUT_DIR="outputs"
mkdir -p $OUTPUT_DIR

# =======================================================================================
# DEBUGGING
# Check if --no-debugging option is passed
NO_DEBUGGING=false
for arg in "$@"; do
    if [ "$arg" == "--no-debugging" ]; then
        NO_DEBUGGING=true
        break
    fi
done

# Skip debugging prompt if --no-debugging is passed
if [ "$NO_DEBUGGING" = true ]; then
    # Run the executable with the input file and redirect output
    EXECUTABLE="./src/sjf_sched"
    OUTPUT_FILE="$OUTPUT_DIR/$(basename "${INPUT_FILE%.conf}").out"

    $EXECUTABLE $INPUT_FILE > $OUTPUT_FILE
    if [ $? -ne 0 ]; then
        echo "Execution of $EXECUTABLE failed."
        exit 1
    fi
else
    # Prompt the user to choose between normal execution or debugging
    echo ""
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
    fi
fi

# =======================================================================================
# PLOT
# Check if --no-plot option is passed
NO_PLOT=false
for arg in "$@"; do
    if [ "$arg" == "--no-plot" ]; then
        NO_PLOT=true
        break
    fi
done

# Run the plot.py script with the generated .out file if --no-plot is not passed
if [ "$NO_PLOT" = false ]; then
    PLOT_SCRIPT="plot.py"
    PLOT_ARGS="$OUTPUT_FILE"

    # Add --no-goodness option if the make target is no-goodness
    if [ "$MAKE_TARGET" == "no-goodness" ]; then
        PLOT_ARGS="$PLOT_ARGS --no-goodness"
    fi

    python3 $PLOT_SCRIPT $PLOT_ARGS
    if [ $? -ne 0 ]; then
        echo "Execution of $PLOT_SCRIPT failed."
        exit 1
    fi
fi

echo ""
echo "Process completed successfully. Output saved to $OUTPUT_FILE."
