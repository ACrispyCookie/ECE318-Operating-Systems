import os
import re
import argparse
import matplotlib
import matplotlib.pyplot as plt
import numpy as np

from matplotlib.lines import Line2D
from collections import defaultdict


def parse_intervals(file_path):
    """Parse process execution intervals, sleep times, wake-up times, and creation times from the file."""
    process_intervals = defaultdict(list)
    sleep_times = defaultdict(list)
    wake_up_times = defaultdict(list)
    creation_times = defaultdict(list)
    active_process = None
    start_time = None

    with open(file_path, 'r') as file:
        for line in file:
            in_match = re.search(r'\(([^)]+)\)/-?\d+/(\d+)ms - Switching Process In', line)
            sleep_match = re.search(r'\(([^)]+)\)/-?\d+/(\d+)ms - Going to Sleep', line)
            wake_up_match = re.search(r'\(([^)]+)\)/-?\d+/(\d+)ms - Waking Up from Sleep', line)
            creation_match = re.search(r'(\d+)ms - Created task: \(([^)]+)\)', line)

            if in_match:
                process_info, timestamp = in_match.groups()
                timestamp = int(timestamp)

                if active_process is not None and start_time is not None:
                    process_intervals[active_process].append((start_time, timestamp))

                active_process = process_info
                start_time = timestamp

            elif sleep_match:
                process_info, timestamp = sleep_match.groups()
                sleep_times[process_info].append(int(timestamp))

            elif wake_up_match:
                process_info, timestamp = wake_up_match.groups()
                wake_up_times[process_info].append(int(timestamp))

            elif creation_match:
                timestamp, process_info = creation_match.groups()
                creation_times[process_info].append(int(timestamp))

        if active_process is not None and start_time is not None:
            process_intervals[active_process].append((start_time, timestamp))

    return process_intervals, sleep_times, wake_up_times, creation_times


def parse_goodness_scores(file_path):
    """Parses goodness scores from the file and returns a dictionary of scores and a list of timestamps."""
    goodness_data = defaultdict(list)
    timestamps = []

    with open(file_path, 'r') as file:
        for line in file:
            match = re.search(r"(\d+)ms - Goodness scores: (.+)", line)
            if not match:
                continue

            timestamp = int(match.group(1))
            scores_str = match.group(2)

            for score_match in re.finditer(r"\(\(([^)]+)\), ([\d.]+)\)", scores_str):
                process = score_match.group(1)
                value = float(score_match.group(2))
                goodness_data[process].append((timestamp, value))

        timestamps.append(timestamp)

    return goodness_data, timestamps


def parse_expected_bursts(file_path):
    """Parse expected CPU burst lengths for each process."""
    expected_burst_data = defaultdict(list)
    timestamps = []

    with open(file_path, 'r') as file:
        for line in file:
            match = re.search(r'(\d+)ms - Expected bursts: (.+)', line)
            if not match:
                continue

            timestamp, bursts = match.groups()
            timestamp = int(timestamp)
            timestamps.append(timestamp)

            for proc in re.finditer(r'\(\(([^:]+):(\d+)\), (\d+)\)', bursts):
                name, pid, burst = proc.groups()
                expected_burst_data[(name, int(pid))].append((timestamp, int(burst)))

    return expected_burst_data, timestamps


def plot_gantt(process_intervals, sleep_times, wake_up_times, creation_times, image_path):
    """Plot a Gantt chart of process execution, sleep times, wake-up times, and creation times."""
    fig, ax = plt.subplots(figsize=(10, 6))
    yticks, ylabels = [], []

    # Sort processes by ID (extracted from the process name)
    sorted_processes = sorted(process_intervals.items(), key=lambda x: int(x[0].split(':')[1]))

    for i, (process, intervals) in enumerate(sorted_processes):
        yticks.append(i)
        ylabels.append(process)

        # Plot execution intervals
        for start, end in intervals:
            ax.broken_barh([(start, end - start)], (i - 0.4, 0.8), facecolors='tab:blue')

        # Plot sleep times
        for sleep_time in sleep_times.get(process, []):
            ax.plot(sleep_time, i, 'r^', label='Sleep' if i == 0 else "")

        # Plot wake-up times
        for wake_up_time in wake_up_times.get(process, []):
            ax.plot(wake_up_time, i, 'y^', label='Wake Up' if i == 0 else "")

        # Plot creation times
        for creation_time in creation_times.get(process, []):
            ax.plot(creation_time, i, 'g^', label='Created' if i == 0 else "")

    ax.set_yticks(yticks)
    ax.set_yticklabels(ylabels)
    ax.set_xlabel('Time (ms)')
    ax.set_title('Process Execution Timeline')
    ax.legend(handles=[
        Line2D([], [], color='green', marker='^', linestyle='None', label='Created'),
        Line2D([], [], color='yellow', marker='^', linestyle='None', label='Wake Up'),
        Line2D([], [], color='red', marker='^', linestyle='None', label='Sleep'),
    ], loc='upper right')

    # Add more discrete lines on the x-axis
    # max_time = max(end for intervals in process_intervals.values() for _, end in intervals)
    # x_ticks = range(0, max_time + 1, 20)
    # ax.set_xticks(x_ticks)
    # ax.grid(axis='x', linestyle='--', alpha=0.7)
    plt.xticks(rotation=90)

    plt.tight_layout()
    plt.savefig(image_path, dpi=800)
    print(f"Gantt plot saved to {image_path}")


def plot_scatter(data, ylabel, title, image_path, jitter=0.5, log_scale=False, linestyle='None'):
    """Generic scatter plot for time series data per process."""
    plt.figure(figsize=(12, 6))

    for process, entries in data.items():
        if len(entries) == 1:
            # Handle single data point
            times, values = [entries[0][0]], [entries[0][1]]
        else:
            # Unpack multiple data points
            times, values = zip(*entries)

        label = f"{process[0]}:{process[1]}" if isinstance(process, tuple) else process

        # Add jitter to x and y values
        jittered_times = np.array(times) + np.random.uniform(-jitter, jitter, len(times))
        jittered_values = np.array(values) + np.random.uniform(0, jitter, len(values))

        plt.plot(jittered_times, jittered_values, marker='o', linestyle=linestyle, label=label)

    plt.xlabel('Time (ms)')
    plt.ylabel(ylabel)
    if log_scale:
        plt.yscale('log')
    plt.title(title)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend()

    # max_time = max(max(entry[0] for entry in entries) for entries in data.values())
    # x_tick_interval = max_time / 10
    # x_ticks = np.arange(0, max_time + 1, x_tick_interval)
    plt.xticks(rotation=90)

    plt.tight_layout()
    plt.savefig(image_path, dpi=800)
    print(f"{title} saved to {image_path}")


def main():
    matplotlib.use('TkAgg')

    parser = argparse.ArgumentParser(description="Plot scheduler data from .out file")
    parser.add_argument("file_path", help="Path to the .out file")
    parser.add_argument("--no-goodness", action="store_true", help="Skip goodness score plot")
    args = parser.parse_args()

    output_folder = 'plots/sjf/' if args.no_goodness else 'plots/sjf-mod/'
    os.makedirs(output_folder, exist_ok=True)
    filename = os.path.basename(args.file_path).replace('.out', '')

    # Gantt Plot
    intervals, sleeps, wake_ups, creations = parse_intervals(args.file_path)
    gantt_path = os.path.join(output_folder, f"{filename}-gantt.png")
    plot_gantt(intervals, sleeps, wake_ups, creations, gantt_path)

    # Goodness Score Plot
    if not args.no_goodness:
        goodness_data, ts = parse_goodness_scores(args.file_path)
        goodness_path = os.path.join(output_folder, f"{filename}-goodness.png")
        plot_scatter(goodness_data, "Goodness Score", "Goodness Scores per Process over Time", goodness_path, jitter=0.1, log_scale=True, linestyle='-')

    # Expected Burst Plot
    burst_data, ts = parse_expected_bursts(args.file_path)
    burst_path = os.path.join(output_folder, f"{filename}-expected_burst.png")
    plot_scatter(burst_data, "Expected Burst", "Expected Bursts per Process over Time", burst_path)


if __name__ == "__main__":
    main()
