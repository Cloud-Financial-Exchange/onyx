import csv
import matplotlib
import matplotlib.pyplot as plt
from collections import defaultdict
import datetime
import sys
import numpy as np
from matplotlib.ticker import FuncFormatter
from pacing import read_pacing_data, plot_pacing_data
from matplotlib.font_manager import FontProperties

# Create a font object
custom_font = FontProperties(size=8, style="italic")
matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

LINE_WIDTH = 1.5
FONT_SIZE = 9  # 14
# FONT_SIZE = 12  # 14

LINESTYLES = ['-', '--', '-.']
COLORS = ['green', 'red', 'black']
# COLORS = ['green', 'black', 'red']
MARKERS = ['^', '*', 'o']

# EXPECTED_THPT = 70000
# BURSTS = [[5,6], [10,11], [15,16]]
BURSTS = [[3,4], [6,7], [9,10], [12, 13], [15, 16], [18, 19]]
# BURSTS = []
INPUT_END = 20
# BURSTS = []

labels = {
    "mlogs_onyx_jan26_247.csv": "Onyx",
    "mlogs_dbo_jan26_216.csv": "DBO",
    "mlogs_cloudex_jan26_232.csv": "CloudEx",

    # "alogs_notree_simple_jan28_1158.csv": "No Tree",
    # "alogs_tree_simple_jan28_1216.csv": "Tree",

    "alogs_tree_simple_jan28_1216.csv": "No Sequencer",
    "alogs_tree_gfifo_jan28_210.csv": "Sequencer"
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

    # return f"{qt} {ot}"
    return f"{qt} {ot}" + filename

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
        # if "alogs" in file_path:
        #     lw = 0.25
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
            # linestyle=LINESTYLES[category],
            # color=COLORS[category],
            linewidth=LINE_WIDTH
        )
    return relative_seconds[-1] if len(relative_seconds) else -1

if __name__ == "__main__":
    file_paths = sys.argv[1:]

    fig, ax1 = plt.subplots(1, 1, figsize=(5, 2), sharex=True)
    fig.metadata = {
        "command": str(file_paths)
    }

    ends = []
    category = 0
    for file_path in file_paths:
        end = handle_file(file_path, category, "throughput", ax1)
        ends.append(end)
        category += 1
        print()
        print()

    ax1.set_ylabel('Throughput (pkts/sec)', fontsize=FONT_SIZE)
    # ax1.set_ylabel('Order Matching\n Rate (per second)', fontsize=FONT_SIZE)
    # ax1.legend(fontsize=FONT_SIZE)
    ax1.grid(True)
    ax1.tick_params(axis='y', rotation=45, labelsize=FONT_SIZE)
    # ax1.set_ylim(10000, 100000)
    ax1.yaxis.set_major_formatter(FuncFormatter(lambda x, _: f'{int(x/1000)}k'))
    # ax1.set_yscale('log')

    for burst in BURSTS:
        ax1.axvspan(burst[0], burst[1], color='gray', alpha=0.3)

    ax1.set_xlabel('Timeline (seconds)', fontsize=FONT_SIZE)
    ax1.legend(fontsize=FONT_SIZE, loc="lower left")
    # ax1.legend()


    ax1.axvspan(INPUT_END, INPUT_END+0.1, color='black')  # for input end
    x_pos = 20
    y_pos = 6 * 10**4
    y_pos = 1100000
    plt.annotate(
        "MPs stop here",  # Text
        xy=(x_pos, y_pos),  # Arrow's tip (x=20, y=400)
        xytext=(x_pos+2, y_pos),  # Text position (slightly to the right)
        arrowprops=dict(facecolor='black', arrowstyle="->"),  # Arrow properties
        fontsize=9,
        fontproperties=custom_font,
        bbox=dict(facecolor='lightyellow', edgecolor='black', alpha=0.8),  # Textbox style
    )

    if (len(ends) > 0):
        ax1.axvspan(ends[0], ends[0]+0.1, color='green')  # for input end
        x_pos = ends[0]
        y_pos = 600000
        plt.annotate(
            "`No Seq.` receives\nall pkts",  # Text
            # "`Tree` receives\nall pkts",  # Text
            xy=(x_pos, y_pos),  # Arrow's tip (x=20, y=400)
            xytext=(x_pos+2, y_pos),  # Text position (slightly to the right)
            arrowprops=dict(facecolor='green', arrowstyle="->"),  # Arrow properties
            fontsize=9,
            fontproperties=custom_font,
            bbox=dict(facecolor='lightyellow', edgecolor='green', alpha=0.8),  # Textbox style
        )

    if (len(ends) > 1):
        ax1.axvspan(ends[1], ends[1]+0.1, color='red')  # for input end
        x_pos = ends[1]
        y_pos = 250000
        plt.annotate(
            "`Seq.`\nreceives all pkts",  # Text
            # "`No Tree`\nreceives all pkts",  # Text
            xy=(x_pos, y_pos),  # Arrow's tip (x=20, y=400)
            xytext=(x_pos+2, y_pos),  # Text position (slightly to the right)
            arrowprops=dict(facecolor='red', arrowstyle="->"),  # Arrow properties
            fontsize=9,
            fontproperties=custom_font,
            bbox=dict(facecolor='lightyellow', edgecolor='red', alpha=0.8),  # Textbox style
        )

    plt.tight_layout()
    results_loc = './figs/'
    plt.savefig(results_loc + '_latency_tmp.pdf')


# In paper:
# python3 windowed_just_thpt.py assets/mlogs_onyx_jan26_247.csv assets/mlogs_dbo_jan26_216.csv assets/mlogs_cloudex_jan26_232.csv

# python3 windowed_just_thpt.py assets/alogs_tree_simple_jan28_1216.csv assets/alogs_tree_gfifo_jan28_210.csv seqiemcers
#  python3 windowed_just_thpt.py assets/alogs_tree_simple_jan28_1216.csv assets/alogs_notree_simple_jan28_1158.csv trees