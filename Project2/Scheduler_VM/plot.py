import re
import sys
import matplotlib.pyplot as plt
from collections import defaultdict

# Parse the "out" file
def parse_out_file(file_path):
    process_intervals = defaultdict(list)  # Store intervals for each process
    active_process = None  # Track the currently active process
    start_time = None  # Track the start time of the active process

    with open(file_path, 'r') as file:
        for line in file:
            # Match lines with process execution info
            match = re.search(r'\(([^)]+)\)/(\d+)/(\d+)ms - Switching Process In', line)
            if match:
                process_info, _, timestamp = match.groups()
                process_name = process_info.split(':')[0]  # Extract process name
                timestamp = int(timestamp)

                # If a process is already active, close its interval
                if active_process is not None and start_time is not None:
                    process_intervals[active_process].append((start_time, timestamp))

                # Update the active process and start time
                active_process = process_name
                start_time = timestamp

        # Close the last active process interval if any
        if active_process is not None and start_time is not None:
            process_intervals[active_process].append((start_time, timestamp))

    return process_intervals

# Generate Gantt-like graph
def plot_gantt(process_intervals, image_path='plot.png'):
    fig, ax = plt.subplots(figsize=(10, 6))
    yticks = []
    ylabels = []
    for i, (process, intervals) in enumerate(process_intervals.items()):
        yticks.append(i)
        ylabels.append(process)
        for start_time, end_time in intervals:  # Unpack the start and end times
            ax.broken_barh([(start_time, end_time - start_time)], (i - 0.4, 0.8), facecolors='tab:blue')

    ax.set_yticks(yticks)
    ax.set_yticklabels(ylabels)
    ax.set_xlabel('Time (ms)')
    ax.set_title('Gantt Graph of Process Execution')
    plt.grid(axis='x', linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig(image_path, dpi=300)

# Main function
if __name__ == "__main__":
    file_path = sys.argv[1]
    process_intervals = parse_out_file(file_path)

    filename = file_path.split('/')[-1]
    image_path = 'plots/' + filename.replace('.out', '') + '.png'
    plot_gantt(process_intervals, image_path)
