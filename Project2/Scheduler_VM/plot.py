import os
import re
import argparse
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import warnings

from collections import defaultdict
from matplotlib.lines import Line2D
from matplotlib.widgets import CheckButtons

# ========== PARSERS ==========

def parse_intervals(file_path):
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
            timeslice_match = re.search(r'Selected Timeslice: (\d+)', line)

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

            elif timeslice_match:
                timeslice = timeslice_match.group(1)

        if active_process and start_time is not None:
            process_intervals[active_process].append((start_time, last_timestamp))

    return process_intervals, sleep_times, wake_up_times, creation_times, timeslice


def parse_goodness_scores(file_path):
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

def plot_cpu_usage(intervals, image_path, slice_size=10):
    all_intervals = [(s, e) for proc, ranges in intervals.items() if 'IO' not in proc and 'Init' not in proc for s, e in ranges]
    if not all_intervals:
        print("No non-IO processes to analyze.")
        return None
    max_time = max(e for _, e in all_intervals)
    num_slices = (max_time + slice_size - 1) // slice_size
    usage = np.zeros(num_slices)
    for start, end in all_intervals:
        s_idx, e_idx = start // slice_size, (end - 1) // slice_size
        for i in range(s_idx, e_idx + 1):
            slice_start = i * slice_size
            overlap = min(end, slice_start + slice_size) - max(start, slice_start)
            usage[i] += overlap
    percent = (usage / slice_size) * 100
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.plot(range(len(percent)), percent, marker='o', linestyle='-')
    ax.fill_between(range(len(percent)), percent, alpha=0.1, color='blue')
    ax.set_title(f"Non-IO CPU Usage (per {slice_size} ms slice)")
    ax.set_xlabel(f"Slice index (each = {slice_size} ms)")
    ax.set_ylabel("CPU Usage (%)")
    ax.set_ylim(0, 100)
    ax.grid(True, linestyle='--', alpha=0.7)
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", message=".*not compatible with tight_layout.*")
        plt.tight_layout()
    plt.savefig(image_path, dpi=800)
    print(f"CPU usage plot saved to {image_path}")
    return fig


def plot_scatter(data, ylabel, title, image_path, jitter=0.5,
                 log_scale=False, linestyle='None', toggle_figs=True,
                 xrotation=90, show_legend=True, force_unique_colors=False,
                 interval=1):
    fig, ax = plt.subplots(figsize=(12, 6))
    lines, labels = [], []

    if force_unique_colors:
        color_palettes = [plt.cm.tab20.colors, plt.cm.tab10.colors, plt.cm.Set3.colors]
        combined_colors = [color for palette in color_palettes for color in palette]
        if len(data) > len(combined_colors):
            extra_colors = plt.cm.viridis(np.linspace(0, 1, len(data) - len(combined_colors)))
            combined_colors.extend(extra_colors)
        color_map = {proc: combined_colors[i % len(combined_colors)] for i, proc in enumerate(data.keys())}
    else:
        color_map = {}

    def aggregate(entries, interval):
        if interval <= 1:
            return entries
        buckets = defaultdict(list)
        for t, val in entries:
            key = t // interval
            buckets[key].append(val)
        return [(k * interval, np.mean(v)) for k, v in sorted(buckets.items())]

    for i, (proc, entries) in enumerate(data.items()):
        reduced = aggregate(entries, interval)
        times, values = zip(*reduced) if len(reduced) > 1 else ([reduced[0][0]], [reduced[0][1]])
        lbl = f"{proc[0]}:{proc[1]}" if isinstance(proc, tuple) else proc
        xj = np.random.uniform(-jitter, jitter, len(times))
        yj = np.random.uniform(0, jitter, len(values))
        line, = ax.plot(np.array(times) + xj, np.array(values) + yj,
                        marker='o',
                        linestyle=linestyle if linestyle != 'None' else '',
                        label=lbl,
                        color=color_map.get(proc))
        lines.append(line)
        labels.append(lbl)

    ax.set_xlabel("Time (ms)")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    if log_scale:
        ax.set_yscale('log')
    ax.grid(True, linestyle='--', alpha=0.7)
    plt.xticks(rotation=xrotation)

    if toggle_figs:
        rax = plt.axes([0.78, 0.2, 0.2, 0.6])
        visibility = [True] * len(lines)
        chk = CheckButtons(rax, labels, visibility)

        def toggle(label):
            idx = labels.index(label)
            lines[idx].set_visible(not lines[idx].get_visible())
            fig.canvas.draw_idle()

        chk.on_clicked(toggle)
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", message=".*not compatible with tight_layout.*")
            plt.tight_layout(rect=[0, 0, 0.75, 1])
    elif show_legend:
        ax.legend(loc='center left', bbox_to_anchor=(1, 0.5))
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", message=".*not compatible with tight_layout.*")
            plt.tight_layout(rect=[0, 0, 1, 1])
    else:
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", message=".*not compatible with tight_layout.*")
            plt.tight_layout()

    plt.savefig(image_path, dpi=800)
    print(f"{title} saved to {image_path}")
    return fig


def plot_gantt(intervals, sleeps, wakeups, creations, image_path, timeslice,
               show_spawn=True, show_sleep=True, show_wake=True, xrotation=90):
    fig, ax = plt.subplots(figsize=(10, 6))
    color_cycle = plt.rcParams['axes.prop_cycle'].by_key()['color']
    yticks, ylabels = [], []
    procs = sorted(intervals.items(), key=lambda x: int(x[0].split(":")[1]))

    for i, (proc, times) in enumerate(procs):
        yticks.append(i)
        ylabels.append(proc)
        for start, end in times:
            ax.broken_barh([(start, end - start)], (i - 0.4, 0.8), facecolors=color_cycle[i % len(color_cycle)])
        if show_spawn:
            for t in creations.get(proc, []):
                ax.plot(t, i, 'g^', label='Spawn' if i == 0 else "")
        if show_sleep:
            for t in sleeps.get(proc, []):
                ax.plot(t, i, 'r^', label='Sleep' if i == 0 else "")
        if show_wake:
            for t in wakeups.get(proc, []):
                ax.plot(t, i, 'y^', label='Wake Up' if i == 0 else "")

    ax.set_yticks(yticks)
    ax.set_yticklabels(ylabels)
    ax.set_xlabel('Time (ms)')
    ax.set_title('Process Execution Timeline')
    ax.text(0.99, 0.01, f"Timeslice: {timeslice} jiffies", transform=ax.transAxes,
            fontsize=10, color='gray', ha='right', va='bottom', alpha=0.7)

    legend_handles = []
    if show_spawn:
        legend_handles.append(Line2D([], [], color='green', marker='^', linestyle='None', label='Spawn'))
    if show_sleep:
        legend_handles.append(Line2D([], [], color='red', marker='^', linestyle='None', label='Sleep'))
    if show_wake:
        legend_handles.append(Line2D([], [], color='yellow', marker='^', linestyle='None', label='Wake Up'))
    if legend_handles:
        ax.legend(handles=legend_handles, loc='center left', bbox_to_anchor=(1, 0.5))
        plt.tight_layout(rect=[0, 0, 0.85, 1])

    plt.xticks(rotation=xrotation)
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", message=".*not compatible with tight_layout.*")
        plt.tight_layout()
    plt.savefig(image_path, dpi=800)
    print(f"Gantt plot saved to {image_path}")
    return fig


# ========== MAIN ==========

def main():
    matplotlib.use('TkAgg')
    parser = argparse.ArgumentParser(description="Plot scheduler logs")
    parser.add_argument("file_path", help="Path to .out file")
    parser.add_argument("--no-goodness", action="store_true", help="Skip goodness plot")
    parser.add_argument("--hide", type=str,
                        help="Hide specific plots: g=Gantt, b=Burst, c=CPU, s=Goodness, all=all")
    parser.add_argument("--cpu-graph-slice", type=int, default=500,
                        help="Slice size for CPU usage plot (in ms)")
    parser.add_argument("--burst-graph-slice", type=int, default=3000,
                    help="Interval size for expected burst plot aggregation (in ms)")
    args = parser.parse_args()

    hide_flags = args.hide or ""
    if 'all' in hide_flags: hide_flags = 'gbcs'
    outdir = "plots/sjf-mod/" if not args.no_goodness else "plots/sjf/"
    os.makedirs(outdir, exist_ok=True)
    base = os.path.basename(args.file_path).replace(".out", "")
    intervals, sleeps, wakeups, creations, timeslice = parse_intervals(args.file_path)

    figs = {}
    figs['g'] = plot_gantt(
        intervals, sleeps, wakeups, creations,
        os.path.join(outdir, f"{base}-gantt.png"),
        timeslice,
        show_spawn = True,
        show_sleep = True,
        show_wake  = True,
        xrotation=0
    )

    figs['c'] = plot_cpu_usage(
        intervals,
        os.path.join(outdir, f"{base}-cpu_usage.png"),
        slice_size=args.cpu_graph_slice
    )

    if not args.no_goodness:
        goodness = parse_goodness_scores(args.file_path)
        goodness = {k: v for k, v in goodness.items() if "Init" not in k}
        figs['s'] = plot_scatter(
            goodness,
            "Goodness Score",
            "Goodness Scores over Time",
            os.path.join(outdir, f"{base}-goodness.png"),
            jitter=0.1,
            log_scale=True,
            linestyle='-',
            toggle_figs=False,
            force_unique_colors=True,
            xrotation=0,
            show_legend=True
        )

    bursts = parse_expected_bursts(args.file_path)
    bursts = {k: v for k, v in bursts.items() if "Init" not in k[0]}
    figs['b'] = plot_scatter(
        bursts,
        "Expected Burst",
        "Expected Bursts over Time",
        os.path.join(outdir, f"{base}-expected_burst.png"),
        toggle_figs=False,
        xrotation=0,
        linestyle='-',
        show_legend=True,
        force_unique_colors=True,
        interval = args.burst_graph_slice
    )

    for key, fig in figs.items():
        if key in hide_flags and fig is not None:
            plt.close(fig)
    if any(key not in hide_flags for key in figs):
        plt.show()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print('\nStopping...')
