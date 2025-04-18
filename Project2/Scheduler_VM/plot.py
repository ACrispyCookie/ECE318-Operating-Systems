import os
import re
import argparse
from collections import defaultdict

import matplotlib
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D


def parse_intervals(file_path):
    """Parse process execution intervals and sleep times from the file."""
    process_intervals = defaultdict(list)
    sleep_times = defaultdict(list)
    active_process = None
    start_time = None

    with open(file_path, 'r') as file:
        for line in file:
            in_match = re.search(r'\(([^)]+)\)/-?\d+/(\d+)ms - Switching Process In', line)
            sleep_match = re.search(r'\(([^)]+)\)/-?\d+/(\d+)ms - Going to Sleep', line)

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

        if active_process is not None and start_time is not None:
            process_intervals[active_process].append((start_time, timestamp))

    return process_intervals, sleep_times


def parse_goodness_scores(file_path):
    """Parse goodness scores for each process."""
    goodness_data = defaultdict(list)
    timestamps = []

    with open(file_path, 'r') as file:
        for line in file:
            match = re.search(r'(\d+)ms - Goodness scores: (.+)', line)
            if not match:
                continue

            timestamp, scores = match.groups()
            timestamp = int(timestamp)
            timestamps.append(timestamp)

            for proc in re.finditer(r'\(\(([^)]+)\), ([\d.]+)\)', scores):
                process_name, goodness = proc.groups()
                goodness_data[process_name].append((timestamp, float(goodness)))

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


def plot_gantt(process_intervals, sleep_times, image_path):
    """Plot a Gantt chart of process execution and sleep times."""
    fig, ax = plt.subplots(figsize=(10, 6))
    yticks, ylabels = [], []

    for i, (process, intervals) in enumerate(process_intervals.items()):
        yticks.append(i)
        ylabels.append(process)

        for start, end in intervals:
            ax.broken_barh([(start, end - start)], (i - 0.4, 0.8), facecolors='tab:blue')

        for sleep_time in sleep_times.get(process, []):
            ax.plot(sleep_time, i, 'r^', label='Sleep' if i == 0 else "")

    ax.set_yticks(yticks)
    ax.set_yticklabels(ylabels)
    ax.set_xlabel('Time (ms)')
    ax.set_title('Process Execution Timeline')
    ax.legend(handles=[Line2D([], [], color='red', marker='^', linestyle='None', label='Sleep')], loc='upper right')
    plt.grid(axis='x', linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig(image_path, dpi=800)
    print(f"Gantt plot saved to {image_path}")


def plot_scatter(data, ylabel, title, image_path):
    """Generic scatter plot for time series data per process."""
    plt.figure(figsize=(12, 6))

    for process, entries in data.items():
        times, values = zip(*entries)
        label = f"{process[0]}:{process[1]}" if isinstance(process, tuple) else process
        plt.plot(times, values, marker='o', linestyle='None', label=label)

    plt.xlabel('Time (ms)')
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.legend()
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
    intervals, sleeps = parse_intervals(args.file_path)
    gantt_path = os.path.join(output_folder, f"{filename}-gantt.png")
    plot_gantt(intervals, sleeps, gantt_path)

    # Goodness Score Plot
    if not args.no_goodness:
        goodness_data, ts = parse_goodness_scores(args.file_path)
        goodness_path = os.path.join(output_folder, f"{filename}-goodness.png")
        plot_scatter(goodness_data, "Goodness Score", "Goodness Scores per Process over Time", goodness_path)

    # Expected Burst Plot
    burst_data, ts = parse_expected_bursts(args.file_path)
    burst_path = os.path.join(output_folder, f"{filename}-expected_burst.png")
    plot_scatter(burst_data, "Expected Burst", "Expected Bursts per Process over Time", burst_path)


if __name__ == "__main__":
    main()
