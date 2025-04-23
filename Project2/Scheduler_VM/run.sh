#!/bin/bash

# =======================================
# CONFIGURABLE DEFAULTS
CONFS_DIR="confs"
OUTPUT_DIR="outputs"
EXECUTABLE="./src/sjf_sched"
PLOT_SCRIPT="plot.py"

# =======================================
# INITIAL VALUES
INPUT_FILE=""
NO_GOODNESS=false
DEBUG=false
NICE=false
NO_PLOT=false
HIDE=""
TIMESLICE=10
BURST_GRAPH_SLICE=3000
CPU_GRAPH_SLICE=500

# =======================================
# USAGE INFO
print_help() {
    cat <<EOF
Usage: $0 [OPTIONS]

Options:
  --config=FILE            Use specified configuration file
  --no-goodness            Disable the goodness algorithm
  --debug                  Enable debugging using gdb
  --no-plot                Do not run the plot script
  --nice                   Calculate timeslice for each task based on its 'NICE' value
  --timeslice=N            (Default: N=$TIMESLICE) Timeslice value for each process (in jiffies)
  --hide=FLAGS             Prevents specific graphs from displaying.
                           Use a combination of the following flags:
                             g   - Hide Gantt chart
                             b   - Hide Expected Burst graph
                             c   - Hide CPU Usage graph
                             s   - Hide Goodness Score graph
                             all - Hide all graphs
  --cpu-graph-slice=N      (Default: N=$CPU_GRAPH_SLICE) Set the slice size for the CPU graph (in ms)
  --burst-graph-slice=N    (Default: N=$BURST_GRAPH_SLICE) Set the slice size for the Burst graph (in ms)
  --help                   Show this help message
EOF
}

# =======================================
# PARSE ARGUMENTS
for arg in "$@"; do
    case "$arg" in
        --config=*)      INPUT_FILE="${arg#--config=}" ;;
        --no-goodness)   NO_GOODNESS=true ;;
        --debug)         DEBUG=true ;;
        --nice)          NICE=true ;;
        --no-plot)       NO_PLOT=true ;;
        --hide=*)        HIDE="${arg#--hide=}" ;;
        --burst-graph-slice=*) BURST_GRAPH_SLICE="${arg#--burst-graph-slice=}" ;;
        --timeslice=*)   TIMESLICE="${arg#--timeslice=}" ;;
        --cpu-graph-slice=*) CPU_GRAPH_SLICE="${arg#--cpu-graph-slice=}" ;;
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
# GOODNESS ALGORITHM & NICE VALUE HANDLING
if [ "$NO_GOODNESS" = true ]; then
    MAKE_TARGET="no-goodness"
elif [ "$NICE" = true ]; then
    MAKE_TARGET="nice"
else
    MAKE_TARGET="all"
fi

echo ""
echo "################# Start compilation process #################"
echo ""

make -C src "$MAKE_TARGET" TIMESLICE="$TIMESLICE"
if [ $? -ne 0 ]; then
    echo "Make command failed."
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
OUTPUT_FILE="$OUTPUT_DIR/$(basename "${INPUT_FILE%.conf}").out"

# =======================================
# DEBUGGING / EXECUTION
if [ "$DEBUG" = true ]; then
    gdb --args "$EXECUTABLE" "$INPUT_FILE"
    exit 0
else
    $EXECUTABLE "$INPUT_FILE" > "$OUTPUT_FILE"
fi

if [ $? -ne 0 ]; then
    echo "Execution failed."
    exit 1
fi

# =======================================
# PLOTTING
if [ "$NO_PLOT" = false ]; then
    echo ""
    echo "#################  Start plotting process  ##################"
    echo ""
    PLOT_ARGS="$OUTPUT_FILE"
    [ "$MAKE_TARGET" == "no-goodness" ] && PLOT_ARGS="$PLOT_ARGS --no-goodness"
    [ -n "$HIDE" ] && PLOT_ARGS="$PLOT_ARGS --hide=$HIDE"
    [ -n "$CPU_GRAPH_SLICE" ] && PLOT_ARGS="$PLOT_ARGS --cpu-graph-slice=$CPU_GRAPH_SLICE"
    [ -n "$BURST_GRAPH_SLICE" ] && PLOT_ARGS="$PLOT_ARGS --burst-graph-slice=$BURST_GRAPH_SLICE"

    python3 "$PLOT_SCRIPT" $PLOT_ARGS
    if [ $? -ne 0 ]; then
        echo "Plotting failed."
        exit 1
    fi
fi

echo ""
echo "✅ Process completed successfully. Output saved to $OUTPUT_FILE"
