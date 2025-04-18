#!/bin/bash

# =======================================
# CONFIGURABLE DEFAULTS
CONFS_DIR="confs"
OUTPUT_DIR="outputs"
EXECUTABLE="./src/sjf_sched"
PLOT_SCRIPT="plot.py"

# =======================================
# USAGE INFO
print_help() {
    cat <<EOF
Usage: $0 [OPTIONS]

Options:
  --config=FILE        Use specified configuration file from $CONFS_DIR
  --no-goodness        Disable the goodness algorithm
  --no-debugging       Skip GDB prompt and run normally
  --no-plot            Do not run the plot script
  --help               Show this help message
EOF
}

# =======================================
# INITIAL VALUES
INPUT_FILE=""
NO_GOODNESS=false
NO_DEBUGGING=false
NO_PLOT=false

# =======================================
# PARSE ARGUMENTS
for arg in "$@"; do
    case "$arg" in
        --config=*)      INPUT_FILE="${arg#--config=}" ;;
        --no-goodness)   NO_GOODNESS=true ;;
        --no-debugging)  NO_DEBUGGING=true ;;
        --no-plot)       NO_PLOT=true ;;
        --help)          print_help; exit 0 ;;
        *) echo "Unknown option: $arg"; print_help; exit 1 ;;
    esac
done

# =======================================
# CONFIG FILE SELECTION
if [ -z "$INPUT_FILE" ]; then
    if [ ! -d "$CONFS_DIR" ]; then
        echo "The directory '$CONFS_DIR' does not exist."
        exit 1
    fi

    CONF_FILES=("$CONFS_DIR"/*.conf)
    if [ ${#CONF_FILES[@]} -eq 0 ]; then
        echo "No .conf files found in '$CONFS_DIR'."
        exit 1
    fi

    echo "Available configuration files:"
    for i in "${!CONF_FILES[@]}"; do
        echo "$((i + 1)). $(basename "${CONF_FILES[$i]}")"
    done

    read -p "Enter the number of the configuration file to use: " SELECTION
    if ! [[ "$SELECTION" =~ ^[0-9]+$ ]] || [ "$SELECTION" -lt 1 ] || [ "$SELECTION" -gt "${#CONF_FILES[@]}" ]; then
        echo "Invalid selection."
        exit 1
    fi

    INPUT_FILE="${CONF_FILES[$((SELECTION - 1))]}"
fi

if [ ! -f "$INPUT_FILE" ]; then
    echo "The file '$INPUT_FILE' does not exist."
    exit 1
fi

# =======================================
# GOODNESS ALGORITHM
if [ "$NO_GOODNESS" = true ]; then
    MAKE_TARGET="no-goodness"
else
    echo ""
    read -p "Do you want to enable the goodness algorithm? (y/n) [default: y]: " GOODNESS_CHOICE
    if [[ "$GOODNESS_CHOICE" =~ ^[Nn]$ ]]; then
        MAKE_TARGET="no-goodness"
    else
        MAKE_TARGET="all"
    fi
fi

make -C src "$MAKE_TARGET"
if [ $? -ne 0 ]; then
    echo "Make command failed."
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
OUTPUT_FILE="$OUTPUT_DIR/$(basename "${INPUT_FILE%.conf}").out"

# =======================================
# DEBUGGING / EXECUTION
if [ "$NO_DEBUGGING" = true ]; then
    $EXECUTABLE "$INPUT_FILE" > "$OUTPUT_FILE"
else
    echo ""
    read -p "Do you want to debug the program using gdb? (y/n) [default: n]: " DEBUG_CHOICE
    if [[ "$DEBUG_CHOICE" =~ ^[Yy]$ ]]; then
        gdb --args "$EXECUTABLE" "$INPUT_FILE"
        exit 0
    else
        $EXECUTABLE "$INPUT_FILE" > "$OUTPUT_FILE"
    fi
fi

if [ $? -ne 0 ]; then
    echo "Execution failed."
    exit 1
fi

# =======================================
# PLOTTING
if [ "$NO_PLOT" = false ]; then
    PLOT_ARGS="$OUTPUT_FILE"
    [ "$MAKE_TARGET" == "no-goodness" ] && PLOT_ARGS="$PLOT_ARGS --no-goodness"

    python3 "$PLOT_SCRIPT" $PLOT_ARGS
    if [ $? -ne 0 ]; then
        echo "Plotting failed."
        exit 1
    fi
fi

echo ""
echo "✅ Process completed successfully. Output saved to $OUTPUT_FILE"
