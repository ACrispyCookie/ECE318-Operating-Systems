import re
import sys
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from collections import defaultdict

def parse_intervals(file_path):
    process_intervals = defaultdict(list)  # Store intervals for each process
    sleep_times = defaultdict(list)  # Store sleep times for each process
    active_process = None  # Track the currently active process
    start_time = None  # Track the start time of the active process

    with open(file_path, 'r') as file:
        for line in file:
            # Match lines with process execution info
            match = re.search(r'\(([^)]+)\)/(-?\d+)/(\d+)ms - Switching Process In', line)
            if match:
                process_info, _, timestamp = match.groups()
                timestamp = int(timestamp)

                # If a process is already active, close its interval
                if active_process is not None and start_time is not None:
                    process_intervals[active_process].append((start_time, timestamp))

                # Update the active process and start time
                active_process = process_info
                start_time = timestamp

            # Match lines where a process goes to sleep
            sleep_match = re.search(r'\(([^)]+)\)/(-?\d+)/(\d+)ms - Going to Sleep', line)
            if sleep_match:
                process_info, _, timestamp = sleep_match.groups()
                timestamp = int(timestamp)
                sleep_times[process_info].append(timestamp)

        # Close the last active process interval if any
        if active_process is not None and start_time is not None:
            process_intervals[active_process].append((start_time, timestamp))

    return process_intervals, sleep_times


def plot_gantt(process_intervals, sleep_times, image_path='plot.png'):
    fig, ax = plt.subplots(figsize=(10, 6))
    yticks = []
    ylabels = []
    for i, (process, intervals) in enumerate(process_intervals.items()):
        yticks.append(i)
        ylabels.append(process)
        for start_time, end_time in intervals:  # Unpack the start and end times
            ax.broken_barh([(start_time, end_time - start_time)], (i - 0.4, 0.8), facecolors='tab:blue')

        # Plot sleep times as red triangles
        if process in sleep_times:
            for sleep_time in sleep_times[process]:
                ax.plot(sleep_time, i, 'r^', label='Sleep' if i == 0 else "")

    ax.set_yticks(yticks)
    ax.set_yticklabels(ylabels)
    ax.set_xlabel('Time (ms)')
    ax.set_title('Process Execution Timeline')
    ax.legend(handles=[Line2D([], [], color='red', marker='^', linestyle='None', label='Process went to sleep')], loc='upper right')
    plt.grid(axis='x', linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig(image_path, dpi=800)
    print(f"\nGantt plot saved to {image_path}\n")


def parse_goodness_scores(file_path):
    goodness_data = defaultdict(list)  # Store goodness scores for each process
    timestamps = []  # Store timestamps for plotting

    with open(file_path, 'r') as file:
        for line in file:
            # Match lines with goodness scores
            match = re.search(r'(\d+)ms - Goodness scores: (.+)', line)
            if match:
                timestamp, scores = match.groups()
                timestamp = int(timestamp)
                timestamps.append(timestamp)

                # Extract process name and goodness score pairs
                for process_match in re.finditer(r'\(\(([^)]+)\), ([\d.]+)\)', scores):
                    process_name, goodness = process_match.groups()
                    goodness = float(goodness)
                    goodness_data[process_name].append((timestamp, goodness))

    return goodness_data, timestamps


def plot_goodness_chart(goodness_data, timestamps, image_path='goodness_plot.png'):
    plt.figure(figsize=(12, 6))

    # Plot goodness scores for each process
    for process_id, data in goodness_data.items():
        times, scores = zip(*data)  # Separate timestamps and goodness scores
        plt.plot(times, scores, marker='o', linestyle='None', label=f'{process_id}')

    plt.xlabel('Time (ms)')
    plt.ylabel('Goodness Score')
    plt.yscale('log')
    plt.title('Goodness Scores per Process over Time')
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig(image_path, dpi=800)
    print(f"\nGoodness plot saved to {image_path}\n")

def parse_expected_bursts(file_path):
    expected_burst_data = defaultdict(list)  # Store expected bursts for each process
    timestamps = []  # Store timestamps for plotting

    with open(file_path, 'r') as file:
        for line in file:
            # Match lines with expected burst data
            match = re.search(r'(\d+)ms - Expected bursts: (.+)', line)
            if match:
                timestamp, bursts = match.groups()
                timestamp = int(timestamp)
                timestamps.append(timestamp)

                # Extract process name, ID, and expected burst
                for burst_match in re.finditer(r'\(\(([^:]+):(\d+)\), (\d+)\)', bursts):
                    process_name, process_id, expected_burst = burst_match.groups()
                    process_id = int(process_id)
                    expected_burst = int(expected_burst)
                    expected_burst_data[(process_name, process_id)].append((timestamp, expected_burst))

    return expected_burst_data, timestamps


def plot_expected_burst_chart(expected_burst_data, timestamps, image_path='expected_burst_plot.png'):
    plt.figure(figsize=(12, 6))

    # Plot expected bursts for each process
    for (process_name, process_id), data in expected_burst_data.items():
        times, bursts = zip(*data)  # Separate timestamps and expected bursts
        plt.plot(times, bursts, marker='o', linestyle='None', label=f'{process_name}:{process_id}')

    plt.xlabel('Time (ms)')
    plt.ylabel('Expected Burst')
    plt.title('Expected Bursts per Process over Time')
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig(image_path, dpi=800)
    print(f"\nExpected burst plot saved to {image_path}\n")


# Main function
if __name__ == "__main__":
    matplotlib.use('TkAgg')

    # Parse intervals
    file_path = sys.argv[1]
    process_intervals, sleep_times = parse_intervals(file_path)  # Unpack both returned values
    filename = file_path.split('/')[-1]

    image_path = 'plots/' + filename.replace('.out', '') + '-gantt.png'
    plot_gantt(process_intervals, sleep_times, image_path)  # Pass both variables to the function

    # Parse goodness scores
    goodness_data, timestamps = parse_goodness_scores(file_path)

    image_path = 'plots/' + filename.replace('.out', '') + '-goodness.png'
    plot_goodness_chart(goodness_data, timestamps, image_path)

    # Parse expected bursts
    image_path = 'plots/' + filename.replace('.out', '') + '-expected_burst.png'
    expected_burst_data, timestamps = parse_expected_bursts(file_path)
    plot_expected_burst_chart(expected_burst_data, timestamps, image_path)
