import os
import re
import argparse
import matplotlib
import matplotlib.pyplot as plt
import numpy as np

from collections import defaultdict
from matplotlib.lines import Line2D
from matplotlib.widgets import CheckButtons


# ========== PARSERS ==========

def parse_intervals(file_path):
    """Parse process execution, sleep, wake-up, and creation times from the trace log."""
    process_intervals = defaultdict(list)
    sleep_times = defaultdict(list)
    wake_up_times = defaultdict(list)
    creation_times = defaultdict(list)

    active_process = None
    start_time = None
    last_timestamp = None

    with open(file_path, 'r') as file:
        for line in file:
            in_match = re.search(r'\(([^)]+)\)/-?\d+/(\d+)ms - Switching Process In', line)
            sleep_match = re.search(r'\(([^)]+)\)/-?\d+/(\d+)ms - Going to Sleep', line)
            wake_up_match = re.search(r'\(([^)]+)\)/-?\d+/(\d+)ms - Waking Up from Sleep', line)
            creation_match = re.search(r'(\d+)ms - Created task: \(([^)]+)\)', line)

            if in_match:
                process_info, timestamp = in_match.groups()
                timestamp = int(timestamp)
                if active_process and start_time is not None:
                    process_intervals[active_process].append((start_time, timestamp))
                active_process, start_time = process_info, timestamp
                last_timestamp = timestamp

            elif sleep_match:
                process_info, timestamp = sleep_match.groups()
                sleep_times[process_info].append(int(timestamp))

            elif wake_up_match:
                process_info, timestamp = wake_up_match.groups()
                wake_up_times[process_info].append(int(timestamp))

            elif creation_match:
                timestamp, process_info = creation_match.groups()
                creation_times[process_info].append(int(timestamp))

        if active_process and start_time is not None:
            process_intervals[active_process].append((start_time, last_timestamp))

    return process_intervals, sleep_times, wake_up_times, creation_times


def parse_goodness_scores(file_path):
    """Parse goodness scores from the trace log."""
    goodness_data = defaultdict(list)

    with open(file_path, 'r') as file:
        for line in file:
            match = re.search(r"(\d+)ms - Goodness scores: (.+)", line)
            if match:
                timestamp = int(match.group(1))
                scores_str = match.group(2)
                for score_match in re.finditer(r"\(\(([^)]+)\), ([\d.]+)\)", scores_str):
                    proc, score = score_match.groups()
                    goodness_data[proc].append((timestamp, float(score)))

    return goodness_data


def parse_expected_bursts(file_path):
    """Parse expected burst values from the trace log."""
    burst_data = defaultdict(list)

    with open(file_path, 'r') as file:
        for line in file:
            match = re.search(r'(\d+)ms - Expected bursts: (.+)', line)
            if match:
                timestamp = int(match.group(1))
                bursts = match.group(2)
                for proc in re.finditer(r'\(\(([^:]+):(\d+)\), (\d+)\)', bursts):
                    name, pid, burst = proc.groups()
                    burst_data[(name, int(pid))].append((timestamp, int(burst)))

    return burst_data


# ========== PLOTS ==========

def plot_gantt(intervals, sleeps, wakeups, creations, image_path):
    """Plot Gantt chart for process execution and state transitions."""
    fig, ax = plt.subplots(figsize=(10, 6))
    yticks, ylabels = [], []

    sorted_procs = sorted(intervals.items(), key=lambda x: int(x[0].split(":")[1]))

    for i, (proc, times) in enumerate(sorted_procs):
        yticks.append(i)
        ylabels.append(proc)

        for start, end in times:
            ax.broken_barh([(start, end - start)], (i - 0.4, 0.8), facecolors='tab:blue')

        for t in sleeps.get(proc, []):
            ax.plot(t, i, 'r^', label='Sleep' if i == 0 else "")
        for t in wakeups.get(proc, []):
            ax.plot(t, i, 'y^', label='Wake Up' if i == 0 else "")
        for t in creations.get(proc, []):
            ax.plot(t, i, 'g^', label='Created' if i == 0 else "")

    ax.set_yticks(yticks)
    ax.set_yticklabels(ylabels)
    ax.set_xlabel('Time (ms)')
    ax.set_title('Process Execution Timeline')

    ax.legend(handles=[
        Line2D([], [], color='green', marker='^', linestyle='None', label='Created'),
        Line2D([], [], color='yellow', marker='^', linestyle='None', label='Wake Up'),
        Line2D([], [], color='red', marker='^', linestyle='None', label='Sleep'),
    ], loc='upper right')

    plt.xticks(rotation=90)
    plt.tight_layout()
    plt.savefig(image_path, dpi=800)
    print(f"Gantt plot saved to {image_path}")
    return plt


def plot_scatter(data, ylabel, title, image_path, jitter=0.5, log_scale=False, linestyle='None', toggle_figs=True):
    """Plot scatter plot with interactive visibility toggling using CheckButtons."""
    fig, ax = plt.subplots(figsize=(12, 6))

    lines, labels, visibility = [], [], []

    for proc, entries in data.items():
        times, values = zip(*entries) if len(entries) > 1 else ([entries[0][0]], [entries[0][1]])
        label = f"{proc[0]}:{proc[1]}" if isinstance(proc, tuple) else proc

        x_jitter = np.random.uniform(-jitter, jitter, len(times))
        y_jitter = np.random.uniform(0, jitter, len(values))
        line, = ax.plot(
            np.array(times) + x_jitter,
            np.array(values) + y_jitter,
            marker='o',
            linestyle=linestyle if linestyle != 'None' else '',
            label=label
        )
        lines.append(line)
        labels.append(label)
        visibility.append(True)

    ax.set_xlabel("Time (ms)")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    if log_scale:
        ax.set_yscale('log')
    ax.grid(True, linestyle='--', alpha=0.7)
    plt.xticks(rotation=90)

    if toggle_figs:
        # CheckButtons
        rax = plt.axes([0.78, 0.2, 0.2, 0.6])
        check = CheckButtons(rax, labels, visibility)

        def toggle(label):
            idx = labels.index(label)
            lines[idx].set_visible(not lines[idx].get_visible())
            fig.canvas.draw_idle()

        check.on_clicked(toggle)

        plt.tight_layout(rect=[0, 0, 0.75, 1])

    plt.savefig(image_path, dpi=800)
    print(f"{title} saved to {image_path}")

    return plt


# ========== MAIN ==========

def main():
    matplotlib.use('TkAgg')

    parser = argparse.ArgumentParser(description="Plot scheduler logs")
    parser.add_argument("file_path", help="Path to .out file")
    parser.add_argument("--no-goodness", action="store_true", help="Skip goodness plot")
    args = parser.parse_args()

    output_dir = "plots/sjf-mod/" if not args.no_goodness else "plots/sjf/"
    os.makedirs(output_dir, exist_ok=True)
    basename = os.path.basename(args.file_path).replace(".out", "")

    # Gantt Plot
    intervals, sleeps, wakeups, creations = parse_intervals(args.file_path)
    gantt_img = os.path.join(output_dir, f"{basename}-gantt.png")
    plot_gantt(intervals, sleeps, wakeups, creations, gantt_img)

    # Goodness Plot
    if not args.no_goodness:
        goodness = parse_goodness_scores(args.file_path)
        goodness_img = os.path.join(output_dir, f"{basename}-goodness.png")
        plot_scatter(goodness, "Goodness Score", "Goodness Scores over Time", goodness_img,
                     jitter=0.1, log_scale=True, linestyle='-')

    # Expected Burst Plot
    bursts = parse_expected_bursts(args.file_path)
    burst_img = os.path.join(output_dir, f"{basename}-expected_burst.png")
    plot_scatter(bursts, "Expected Burst", "Expected Bursts over Time", burst_img, toggle_figs=False)

    plt.show()

if __name__ == "__main__":
    main()
