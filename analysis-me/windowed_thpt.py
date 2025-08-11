import csv
import matplotlib
import matplotlib.pyplot as plt
from collections import defaultdict
import datetime
import sys
import numpy as np
from matplotlib.ticker import FuncFormatter
from pacing import read_pacing_data, plot_pacing_data

matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

LINE_WIDTH = 2.5
FONT_SIZE = 12  # 14
# FONT_SIZE = 12  # 14

LINESTYLES = ['-', '--']
COLORS = ['green', 'black']
MARKERS = ['^', '*']

# EXPECTED_THPT = 70000
BURSTS = [[5,6], [10,11], [15,16]]
BURSTS = [[3,4], [6,7], [9,10], [12, 13], [15, 16], [18, 19]]
# BURSTS = []

labels = {
    "mlogs_dbo_jan26_216.csv": "DBO",
    "mlogs_cloudex_jan26_232.csv": "CloudEx",
    "mlogs_onyx_jan26_247.csv": "Onyx"
}

def get_label(filename):
    ot = ""
    # if "alogs" in filename:
    #     ot = "all orders"
    # elif "mlogs" in filename:
    #     ot = "matched orders"
    if filename in labels:
        return labels[filename]
    print(filename)

    qt = "SimplePQ"
    if "fancy" in filename or (filename.split('/')[-1])[0] == 'f':
        qt = "LOQ"
    
    if "pacing" in filename:
        ot = "pacing vals"

    return f"{qt} {ot}"
    # return f"{qt} {ot}" + filename

def calculate_median_latency(file_path):
    latencies = []
    with open(file_path, 'r') as file:
        reader = csv.reader(file)
        for row in reader:
            latency = int(row[1])  # Assuming latency is in the second column
            latencies.append(latency)
    return np.median(latencies)

def handle_file(file_path, category, mode, ax):
    throughput = defaultdict(int)
    lowest_ts = -1
    highest_ts = -1
    total_pkts = 0

    latencies = defaultdict(list)
    last_pacing_values = {}

    with open(file_path, 'r') as file:
        reader = csv.reader(file)
        
        start_time = None
        for row in reader:
            timestamp = int(row[0])
            start_time = datetime.datetime.fromtimestamp(timestamp / 1_000_000)
            break

        file.seek(0)

        for row in reader:
            timestamp = int(row[0])
            
            if lowest_ts == -1:
                lowest_ts = timestamp
            else:
                lowest_ts = min(lowest_ts, timestamp)
            
            if highest_ts == -1:
                highest_ts = timestamp
            else:
                highest_ts = max(highest_ts, timestamp)
            
            
            value = int(row[1])  # Assuming pacing value is in the second column
            record_time = datetime.datetime.fromtimestamp(timestamp / 1_000_000)
            relative_second = int((record_time - start_time).total_seconds()) + 1
            throughput[relative_second] += 1
            latencies[relative_second] += [value]
            last_pacing_values[relative_second] = value

            total_pkts += 1

    relative_seconds = list(throughput.keys())
    message_counts = list(throughput.values())
    for s in relative_seconds:
        latencies[s] = list(np.percentile(latencies[s], [25.0, 50.0, 75.0, 99.0]))

    median_latency = calculate_median_latency(file_path)
    print("Median Latency: ", median_latency)
    print("Overall Thpt: ", total_pkts / ((highest_ts-lowest_ts) / 1_000_000))
    burst_thpt = 0
    for b in BURSTS:
        burst_thpt += throughput[b[1]]
    burst_thpt /= len(BURSTS)
    print("Burst Throughput: ", burst_thpt)
    burst_latencies = []
    for b in BURSTS:
        burst_latencies += latencies[b[1]]
    burst_latency = np.median(burst_latencies)
    print("Burst Median Latency: ", burst_latency)
    print("Burst 95th Latency: ", np.percentile(burst_latencies, 95.0))


    label = f"{get_label(file_path.split('/')[-1])}"

    if mode == "throughput":
        lw = LINE_WIDTH
        if "alogs" in file_path:
            lw = 0.25
        l, = ax.plot(
            relative_seconds, 
            message_counts, 
            label=label,
            linestyle=LINESTYLES[category],
            color=COLORS[category],
            linewidth=lw
        )
        if "alogs" in file_path and "sbp" in file_path:
            l.set_dashes([10, 10])

        # ax.axhline(y=EXPECTED_THPT, color='red', linestyle=':', linewidth=0.5)
    else:
        if "alogs" in file_path: return
        ax.plot(
            relative_seconds, 
            [latencies[x][1] for x in relative_seconds], 
            label=f"{label}",
            # label=f"50p, {label}",
            linestyle=LINESTYLES[category],
            color=COLORS[category],
            linewidth=LINE_WIDTH
        )

if __name__ == "__main__":
    file_paths = sys.argv[1:]


    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(4, 4), sharex=True)
    fig.metadata = {
        "command": str(file_paths)
    }

    is_pacing = False
    for file_path in file_paths:
        if "pacing" in file_path:
            is_pacing = True
            break
    if is_pacing:
        fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 5), sharex=True)

    for file_path in file_paths:
        if "pacing" in file_path: continue
        category = 1
        if "fancy" in file_path or (file_path.split('/')[-1])[0] == 'f':
            category = 0
        handle_file(file_path, category, "throughput", ax1)
        print()
        print()

    ax1.set_ylabel('Order Matching\n Rate (per second)', fontsize=FONT_SIZE)
    # ax1.legend(fontsize=FONT_SIZE)
    ax1.grid(True)
    ax1.tick_params(axis='y', rotation=45, labelsize=FONT_SIZE)
    # ax1.set_ylim(10000, 100000)
    ax1.yaxis.set_major_formatter(FuncFormatter(lambda x, _: f'{int(x/1000)}k'))

    for burst in BURSTS:
        ax1.axvspan(burst[0], burst[1], color='gray', alpha=0.3)

    for file_path in file_paths:
        if "pacing" in file_path: continue
        category = 1
        if "fancy" in file_path or (file_path.split('/')[-1])[0] == 'f':
            category = 0
        handle_file(file_path, category, "latency", ax2)

    # ax2.set_xlabel('Relative Seconds', fontsize=FONT_SIZE)
    if not is_pacing:
        ax2.set_xlabel('Relative Seconds', fontsize=FONT_SIZE)
    ax2.set_ylabel('\nOrders Latency \n(\u00b5s)', fontsize=FONT_SIZE)
    ax2.set_yscale('log')
    ax2.legend(fontsize=FONT_SIZE, loc='lower right')
    ax2.tick_params(axis='y', labelsize=FONT_SIZE)
    ax2.tick_params(axis='x', labelsize=FONT_SIZE)
    # ax2.set_ylim(0, 100000)
    ax2.grid(True)
    for burst in BURSTS:
        ax2.axvspan(burst[0], burst[1], color='gray', alpha=0.3)

    if is_pacing:
        for file_path in file_paths:
            if "pacing" not in file_path: continue
            category = 1
            if "fancy" in file_path:
                category = 0
            
            data = read_pacing_data(file_path)
            plot_pacing_data(data, ax3)

        ax3.set_xlabel('Relative Seconds', fontsize=FONT_SIZE)
        ax3.set_ylabel('Pacing', fontsize=FONT_SIZE)
        # ax2.set_yscale('log')
        ax3.set_ylim(0, 50)
        ax3.tick_params(axis='y', labelsize=FONT_SIZE)
        ax3.tick_params(axis='x', labelsize=FONT_SIZE)
        ax3.grid(True)
        for burst in BURSTS:
            ax3.axvspan(burst[0], burst[1], color='gray', alpha=0.3)

    plt.tight_layout()
    results_loc = './figs/'
    plt.savefig(results_loc + '_latency_tmp.pdf')


# Final figure in paper:
# assets/fbp_mlogs_june21_532.csv
# assets/sbp_mlogs_june21_506.csv

# Figure Sep 14, 2024
# assets/sbp_mlogs_june21_506.csv assets/fbp_mlogs_sep14_1207.csv assets/sbp_alogs_june21_506.csv assets/fbp_alogs_sep14_1207.csv


# 1k receivers:
# python3 windowed_thpt.py assets/sbp_alogs_sep18_231.csv  assets/sbp_mlogs_sep18_231.csv assets/fbp_mlogs_sep18_140.csv  assets/fbp_alogs_sep18_140.csv



# May 17, 2025
# python3 windowed_thpt.py assets/simple_slogs_536.csv assets/fancy_slogs_536.csv