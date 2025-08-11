from concurrent.futures import ProcessPoolExecutor
import json
import sys
import os
import numpy as np
import matplotlib.pyplot as plt
import s3
import bootstrap_ci

PREFIX = "30000-h2-variance-parity-based-second-run"
LOCAL_ASSETS_DIR = "./assets/"
TOTAL_RECEIVERS = 100

MARKERS = [".", "*", "o", "^", ',', 'v', '<', '>', '1', '2', '3', '4', '8', 's', 'p', 'P', 'h', 'H', '+', 'x', 'D', 'd']

def get_downloaded_filenames():
    return [os.path.join(LOCAL_ASSETS_DIR, f) for f in os.listdir(LOCAL_ASSETS_DIR) if os.path.isfile(os.path.join(LOCAL_ASSETS_DIR, f))]


def get_data_files():
    data_source = "local"
    if (len(sys.argv) < 2):
        print("Provide data source (local/remote) as first cmd arg")
        exit(1)

    data_source = str(sys.argv[1])

    if (data_source == "remote"):
        data_files = s3.download_s3_data_and_get_downloaded_filenames(PREFIX, -1, LOCAL_ASSETS_DIR, get_downloaded_filenames())
    elif (data_source == "local"):
        data_files = get_downloaded_filenames()
    
    data_files = sorted(data_files)
    return data_files


def get_label():
    if (len(sys.argv) < 3):
        print("Provide label as second cmd arg")
        exit(1)

    return str(sys.argv[2])

def print_info(info):
    print("\033[92m[INFO]: ", info, "\033[0m")

def read_and_deserialize_data(filename: str):
    print_info(f"Reading {filename}")
    data = np.loadtxt(filename, delimiter=',', dtype=int)
    print_info(f"Done reading {filename}")
    return data

def get_max_owd_per_msg_id(filename, max_owd_per_msg_id):
    data = read_and_deserialize_data(filename)
    for row in data:
        msg_id = int(row[0])
        owd = int(row[1])

        if msg_id not in max_owd_per_msg_id:
            max_owd_per_msg_id[msg_id] = (0, 0)

        a = max(max_owd_per_msg_id[msg_id][0], owd)
        b = max_owd_per_msg_id[msg_id][1] + (owd/TOTAL_RECEIVERS) # TODO

        max_owd_per_msg_id[msg_id] = (a, b)
    return max_owd_per_msg_id

def plot_means_and_max_owd_per_msg_id(experimentt_label, max_owd_per_msg_id, texts, index, plot_ci):
    owds = []
    means = []

    for msg_id in max_owd_per_msg_id:
        owds += [max_owd_per_msg_id[msg_id][0]]
        means += [max_owd_per_msg_id[msg_id][1]]

    paired_data = list(zip(owds, means))

    sorted_data = sorted(paired_data, key=lambda x: x[0])

    sorted_owds, sorted_means = zip(*sorted_data)
    steps = 100
    plt.plot(sorted_owds[::steps], sorted_means[::steps], label=f"lower")
    plt.xlabel("OWDS")
    plt.ylabel("Means)")



def plot_max_owd_per_msg_id(experimentt_label, max_owd_per_msg_id, texts, index, plot_ci):
    owds = []
    means = []

    for msg_id in max_owd_per_msg_id:
        owds += [max_owd_per_msg_id[msg_id][0]]
        means += [max_owd_per_msg_id[msg_id][1]]

    percentiles_to_show = list(np.arange(90, 100, 0.1))
    # percentiles_to_show = list(np.arange(1, 91, 1))
    # percentiles_to_show = list(np.arange(1, 100, 1))
    percentiles_to_show = [round(i, 1) for i in percentiles_to_show]
    print(percentiles_to_show)

    x = []
    y = []

    x = percentiles_to_show
    y = np.percentile(owds, x)

    plt.yscale('log')  # Set the y-scale to logarithmic
    line, = plt.plot(x, y, marker=MARKERS[index%len(MARKERS)], label=texts[experimentt_label])
    plt.title("Max OWD Across All Receivers For Different Hedging Schemes")
    plt.xlabel("Percentiles")
    plt.ylabel("OWD (microseconds)")

    line_color = line.get_color()
    if plot_ci:
        ci_lower, ci_upper = bootstrap_ci.get_bootstrap_ci(owds, percentiles_to_show, bootstrap_samples=100)
        plt.fill_between(percentiles_to_show, ci_lower, ci_upper, color=line_color, alpha=1)

    xticks_positions = percentiles_to_show
    xticks_labels = [str(x) for x in xticks_positions]
    for i, ele in enumerate(xticks_labels):
        if i%5 != 0:
            xticks_labels[i] = ''
    xticks_labels[0] = str(xticks_positions[0])
    xticks_labels[-1] = str(xticks_positions[-1])
    plt.xticks(xticks_positions, xticks_labels)


    # Show some text
    # x_to_find = 99.0
    # y_to_find = y[x.index(x_to_find)]

    # plt.text(x_to_find, y_to_find, f"99th percentile = {np.percentile(owds, 99)}", ha='right', color=line_color)



def save_max_owd_per_msg_id(label, max_owd_per_msg_id):
    with open(f"{label}.json", "w") as f:
        f.write(json.dumps(max_owd_per_msg_id))

def merge_results(result, more_result):
    print(len(result), len(more_result))
    keys = list(result.keys()) + list(more_result.keys())
    for key in keys:
        if key not in result:
            result[key] = more_result[key]

        if key in more_result:
            result[key] = (max(result[key][0], more_result[key][0]), result[key][1] + more_result[key][1])
            # result[key][0] = max(result[key][0], more_result[key][0])
            # result[key][1] = result[key][1] + more_result[key][1]
    return result

def main():
    data_files = get_data_files()
    experimentt_label = get_label()

    result = {}
    futures = []
    with ProcessPoolExecutor(max_workers=1) as executor:
        for filename in data_files:
            max_owd_per_msg_id = {} # for each msg, id what is the max owd across all the receivers
            future = executor.submit(get_max_owd_per_msg_id, filename, max_owd_per_msg_id)

            #  extra
            more_result = future.result()    
            result = merge_results(result, more_result)
            future.cancel()
            
        #     futures += [future]

        # for future in futures:
        #     more_result = future.result()    
        #     result = merge_results(result, more_result)
        #     future.cancel()

    save_max_owd_per_msg_id(f"Means-{experimentt_label}", result)
    # plot_max_owd_per_msg_id(experimentt_label, result)
    # plt.savefig(f"{experimentt_label}.pdf")

def plot_all():

    labels = [
        # "10000-h2-variance-parity-based",
        # "10000-h2-dyn-random-parity-based",
        # "10000-h2-fixed-random-parity-based",
        # "10000-h2-dyn-random-and-variance-parity-based-separate-proxies",
        # "0-h3-dyn-random-and-variance-parity-based",
        "30000-h2-variance-parity-based-second-run",
        # "10000-h4-variance-parity-based",
        # "10000-h4-dyn-random-parity-based",
        # "30000-h4-fixed-random-parity-based",
        # "15000-h2-variance-and-dyn-random-parity-based-simaltanous",
        # "90000-h2-variance-and-fixed-random-parity-based-simaltanous",
        # "45000-h2-variance-and-dyn-random-parity-based-simaltanous",
    ]
    texts = {
        "10000-h2-dyn-random-parity-based": "H=2, Dynamic Randomized Hedging",
        "10000-h2-fixed-random-parity-based": "H=2, Fixed Randomized Hedging",
        "10000-h2-variance-parity-based": "H=2, Variance Based Hedging",
        "10000-h4-variance-parity-based": "H=4, Variance Based Hedging",
        "10000-h4-dyn-random-parity-based": "H=4, Dynamic Randmoized Hedging",
        "30000-h4-fixed-random-parity-based": "H=4, Fixed Randomized Hedging",
        "10000-h2-dyn-random-and-variance-parity-based-separate-proxies": "H=2, Mixed (Dyn-Random + Variance)",
        "90000-h2-variance-and-fixed-random-parity-based-simaltanous": "H=2, Simaltanous Fixed Random + Variance",
        "45000-h2-variance-and-dyn-random-parity-based-simaltanous": "H=2, Simaltanous Variance + Dynamic Randomized",
        "30000-h2-variance-parity-based-second-run": "H=2, Variance based secobnd run",
        "0-h3-dyn-random-and-variance-parity-based": "H=3, Mixed (Dyn-Random + Variance)"
    }

    plt.figure(figsize=(15, 7))
    for ind, l in enumerate(labels):
        with open(f"Means-{l}.json") as f:
            data = json.loads(f.read())
        # plot_max_owd_per_msg_id(l, data, texts, ind, plot_ci=False)
        plot_means_and_max_owd_per_msg_id(l, data, texts, ind, plot_ci=False)
        plt.legend()
    # plt.show()
    plt.savefig("figs/comparison3.pdf")
    return

if __name__ == "__main__":
    plot_all()
    # main()
