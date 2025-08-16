import json
import pandas as pd
import matplotlib.pyplot as plt
import losses
import numpy as np
import util
from parity_plots import plot_all
from parity_plots import plot_x_y
from parity_plots import get_max_owd_data_transformation_fn
from parity_plots import get_diff_between_max_min_owd_data_transformation_fn
from parity_plots import get_mean_holding_duration_data_transformation_fn
from parity_plots import get_max_holding_duration_data_transformation_fn

from alt_data import pair, resolve_pairs

from labels import labels, texts, info

from pathlib import Path

# Returns {msg_id: [max owd, min owd, holding time summed across all receivrs]}
def get_data_per_msg_id(filename, result, metadata):
    data = util.read_and_deserialize_data(filename, False, True)
    print("Rows: ", len(data))
    for row in data:
        msg_id = int(row[0])
        owd = int(row[1])
        receiver_id = int(row[2])
        holding_duration = 0
        if len(row) > 3:
            holding_duration = int(row[3])

        if (metadata["total_receivers"]) < (receiver_id+1):
            metadata["total_receivers"] = receiver_id+1

        if msg_id not in result:
            result[msg_id] = [owd, owd, 0, 0, 0]

        curr_max_owd = max(result[msg_id][0], owd)
        curr_min_owd = min(result[msg_id][1], owd)
        cummulative_holding_duration = result[msg_id][2] + holding_duration
        curr_max_holding = max(result[msg_id][3], holding_duration)
        total_receivers_who_have_this_msg = result[msg_id][4] + 1

        result[msg_id] = [curr_max_owd, curr_min_owd, cummulative_holding_duration, curr_max_holding, total_receivers_who_have_this_msg]
    return result, metadata

def post_process_results(result, metadata):
    # Average the holding duration
    for msg_id in result:
        result[msg_id][2] = result[msg_id][2]/metadata["total_receivers"]
    
    return result, metadata

def save_data_per_msg_id(label, max_owd_per_msg_id):
    data_dir = Path(util.DATA_DIR)
    if not data_dir.is_dir():
        data_dir.mkdir(parents=True, exist_ok=True)

    with open(f"{util.DATA_DIR}{label}.json", "w") as f:
        f.write(util.json.dumps(max_owd_per_msg_id))

def main_data():
    data_files = util.get_data_files()
    experimentt_label = util.get_label()

    result = {}
    metadata = {"total_receivers": 0}

    for filename in data_files:
        result, metadata = get_data_per_msg_id(filename, result, metadata)
    
    result, metadata = post_process_results(result, metadata)

    util.print_info(f"Total receivers found: {metadata['total_receivers']}")
    save_data_per_msg_id(experimentt_label, result)

def alt_data():
    data_files = util.get_data_files()
    experimentt_label = util.get_label()
    experimentt_label += "_ALT"

    result = {}
    metadata = {"total_receivers": 0}

    pairs = pair(data_files)
    data_files = []

    for i, p in enumerate(pairs):
        f1 = p[0]
        f2 = p[1]
        print(f1, f2)
        file = resolve_pairs(f1, f2, i)
        data_files.append(file)

    for filename in data_files:
        result, metadata = get_data_per_msg_id(filename, result, metadata)

    result, metadata = post_process_results(result, metadata)

    util.print_info(f"Total receivers found: {metadata['total_receivers']}")
    save_data_per_msg_id(experimentt_label, result)

def cumulative_latency_data():
    all_owds = []
    data_files = util.get_data_files()
    experimentt_label = f"{util.PREFIX}_allowds"

    for filename in data_files:
        data = util.read_and_deserialize_data(filename)
        for row in data:
            all_owds += [int(row[1])]

    save_data_per_msg_id(experimentt_label, all_owds)
    print(len(all_owds))

def cumulative_latency_plot():
    result = {}
    plt.figure()
    PERCENTILES = list(range(0, 100))
    for ind, label in enumerate(labels):
        with open(f"{util.DATA_DIR}{label}_allowds.json") as f:
            data = json.load(f)
        cdf = np.percentile(data, PERCENTILES)
        plot_x_y(cdf, PERCENTILES, data, label, ind, "", False)
    plt.xlim([0, max(cdf)+50])
    plt.xlabel("OWD (micros)")
    plt.ylabel("CDF")
    plt.legend()
    plt.savefig(f"{util.FIGS_DIR}123test.pdf")


def percentage_improvement_plots():
    PERCENTILES_TO_SHOW = list(util.np.arange(1, 100, 1))
    # PERCENTILES_TO_SHOW = list(util.np.arange(90, 100, 0.1))
    PERCENTILES_TO_SHOW = [round(i, 1) for i in PERCENTILES_TO_SHOW]
    util.print_info(f"...{PERCENTILES_TO_SHOW[-10:]}")

    # plot_all(labels, texts, True, util.Graph.CDF, False, PERCENTILES_TO_SHOW, get_max_owd_data_transformation_fn)
    # plot_all(labels, texts, True, util.Graph.CDF, False, PERCENTILES_TO_SHOW, get_diff_between_max_min_owd_data_transformation_fn)
    # plot_all(labels, texts, False, util.Graph.CDF, False, PERCENTILES_TO_SHOW, get_mean_holding_duration_data_transformation_fn)
    # plot_all(labels, texts, False, util.Graph.CDF, False, PERCENTILES_TO_SHOW, get_max_holding_duration_data_transformation_fn)

    plot_all(labels, texts, True, util.Graph.PERCENTAGE_IMPROVEMENT_PER_PERCENTILE, False, PERCENTILES_TO_SHOW, get_max_owd_data_transformation_fn)
    # plot_all(labels, texts, True, util.Graph.PERCENTAGE_IMPROVEMENT_PER_PERCENTILE, False, PERCENTILES_TO_SHOW, get_diff_between_max_min_owd_data_transformation_fn)


def latency_plots():
    PERCENTILES_TO_SHOW = list(util.np.arange(1, 100, 1))
    # PERCENTILES_TO_SHOW = list(util.np.arange(90, 100, 0.5))
    # PERCENTILES_TO_SHOW += [99.9]
    # PERCENTILES_TO_SHOW = [round(i, 2) for i in PERCENTILES_TO_SHOW]
    util.print_info(f"...{PERCENTILES_TO_SHOW[-10:]}")

    plot_all(labels, texts, False, util.Graph.CDF, False, PERCENTILES_TO_SHOW, get_max_owd_data_transformation_fn)
    plot_all(labels, texts, False, util.Graph.CDF, False, PERCENTILES_TO_SHOW, get_diff_between_max_min_owd_data_transformation_fn)
    plot_all(labels, texts, False, util.Graph.CDF, False, PERCENTILES_TO_SHOW, get_mean_holding_duration_data_transformation_fn)
    # plot_all(labels, texts, False, util.Graph.CDF, False, PERCENTILES_TO_SHOW, get_max_holding_duration_data_transformation_fn)

    # plot_all(labels, texts, True, util.Graph.PERCENTAGE_IMPROVEMENT_PER_PERCENTILE, False, PERCENTILES_TO_SHOW, get_max_owd_data_transformation_fn)
    # plot_all(labels, texts, True, util.Graph.PERCENTAGE_IMPROVEMENT_PER_PERCENTILE, False, PERCENTILES_TO_SHOW, get_diff_between_max_min_owd_data_transformation_fn)

def time_series_plots():
    PERCENTILES_TO_SHOW = list(util.np.arange(1, 100, 1))
    plot_all(labels, texts, True, util.Graph.TIME_SERIES, False, PERCENTILES_TO_SHOW, get_max_owd_data_transformation_fn)
    # plot_all(labels, texts, True, util.Graph.TIME_SERIES_VARIANCE, False, PERCENTILES_TO_SHOW, get_diff_between_max_min_owd_data_transformation_fn)


def main_show_per_receiver_latency_abnormality():
    if (len(labels) != 1):
        print("Only 1 label supported per run")
        exit(1)

    parity = info[labels[0]]["parity"]

    percentiles_per_receiver = {}
    percentiles_per_receiver_odd = {}
    percentiles_per_receiver_even = {}

    res = {}
    res_odd = {}
    res_even = {}

    data_files = util.get_data_files()

    for filename in data_files:
        if ".tar.gz" not in filename:
            continue
        data = util.read_and_deserialize_data(filename)

        even_mask = data[:, 0] % 2 == 0
        odd_mask = data[:, 0] % 2 != 0

        data_even = data[even_mask]
        data_odd = data[odd_mask]

        percentiles = [50.0, 90.0, 95.0, 99.0, 99.9]
        res[int(data[:,2][0])] = (np.percentile(data[:, 1], percentiles)).tolist()
        res_odd[int(data_odd[:,2][0])] = (np.percentile(data_odd[:, 1], percentiles)).tolist()
        res_even[int(data_even[:,2][0])] = (np.percentile(data_even[:, 1], percentiles)).tolist()
    
        # print(res)
        # print(res_odd)
        # print(res_even)

    receivers = list(res.keys())
    receivers = sorted(receivers)
    medians = []
    medians_odd = []
    medians_even = []

    statistic_index = 3

    for r in receivers:
        medians += [res[r][statistic_index]]
        print(r, medians[-1])
        percentiles_per_receiver[str(r)] = res[r]

        medians_odd += [res_odd[r][statistic_index]]
        print(r, medians_odd[-1])
        percentiles_per_receiver_odd[str(r)] = res_odd[r]

        medians_even += [res_even[r][statistic_index]]
        print(r, medians_even[-1])
        percentiles_per_receiver_even[str(r)] = res_even[r]

    if parity:
        plot_x_y(receivers, medians_odd, [], "hedging", 0, "", False)
        plot_x_y(receivers, medians_even, [], "no hedging", 1, "", False)
    else:
        plot_x_y(receivers, medians, [], "", 0, "-", False)

    ticklabels = [str(x) for x in receivers]
    for ind, ele in enumerate(ticklabels):
        if ind%10 != 0:
            ticklabels[ind] = ""
    plt.xticks(receivers, ticklabels)

    plt.legend()
    plt.title(f"Effect of Serialization Delay")
    plt.xlabel("Receiver ID")
    plt.ylabel("Median Latency (us)")
    plt.savefig(f"{util.FIGS_DIR}/{labels[0]}.pdf")

    with open("./data/tmp.json", "w") as f:
        f.write(json.dumps(percentiles_per_receiver, indent=4))
    with open("./data/tmp_odd.json", "w") as f:
        f.write(json.dumps(percentiles_per_receiver_odd, indent=4))
    with open("./data/tmp_even.json", "w") as f:
        f.write(json.dumps(percentiles_per_receiver_even, indent=4))


def plot_files_x_y(files=["./data/dpdk_spray_2.json", "./data/tree_spray_2.json",  "./data/tree_fairness_spray_2.json", "./data/tree_hedging_spray_2.json", "./data/jasper_spray_2.json"]):
    labels = ["DU", "Tree", "Tree + Fairness", "Tree + Hedging", "Tree + Hedging + Fariness", ]

    files = ["./data/tmp_even.json", "./data/tmp_odd.json"]
    labels = ["No Hedging, Tree", "Hedging, Tree"]

    # files = ["./data/dpdk_spray_2.json", "./data/tree_spray_2.json",]
    # labels = ["Direct Unicasts", "Tree"]

    statistic_index = 0

    for ind, file in enumerate(files):
        with open(file) as f:
            res = json.loads(f.read())
        
        receivers = list(res.keys())
        receivers = [int(x) for x in receivers]
        receivers = sorted(receivers)
        medians = []
        for r in receivers:
            medians += [res[str(r)][statistic_index]]
            print(r, medians[-1])
        plot_x_y(receivers, medians, [], labels[ind], ind, "", False)

        ticklabels = [str(x) for x in receivers]
        for ind, ele in enumerate(ticklabels):
            if ind%10 != 0:
                ticklabels[ind] = ""
        plt.xticks(receivers, ticklabels)

        # plt.title(f"Analyzing Spatial OWD For A Proxy Tree")
        plt.xlabel("Receiver ID")
        plt.ylabel("50th percentile OWD (us)")

    plt.tight_layout()
    plt.legend(framealpha=0.5, loc="upper left")
    plt.savefig(f"{util.FIGS_DIR}/{labels[0]}_both.pdf")


def plot_drop_rate():
    drops_rate = {}
    for l in labels:
        data = {}
        with open(f"{util.DATA_DIR}{l}.json") as f:
            data = util.json.loads(f.read())

        second_wise_plr = []
        second_wise_plr_lost_by_all = []
        count = 0 # messages which have been received by `num_receivers` receivers
        tmp = info[l]["msg_rate"]
        all_msg_ids = [int(x) for x in list(data.keys())]

        if ("starting_id" in info[l]):
            ending_id = info[l]["starting_id"] + (info[l]["msg_rate"] * info[l]["total_seconds"])
            all_msg_ids = list(range(info[l]["starting_id"], ending_id))

        dropped_by_all = []
        all_msg_ids = sorted(all_msg_ids)
        all_msg_ids = [str(x) for x in all_msg_ids]
        print("Message Ids in ", l, len(all_msg_ids))
        for msg_id in all_msg_ids:
            if (msg_id not in data) or data[msg_id][4] == 0:
                dropped_by_all += [msg_id]
            else:
                if (len(data[msg_id]) < 5):
                    print("Not supported by data")

                if data[msg_id][4] >= info[l]["num_receivers"]:
                    count += 1
                else:
                    print("bad msh received by only: ", data[msg_id][4])
                    print(msg_id)
                # else:
                #     print(data[msg_id][4])

            tmp -= 1

            if tmp == 0:  # the last second might not get counted, we do not want to partially count it anyway
                expected_messages = info[l]["msg_rate"]
                lost_messages = expected_messages - count
                packet_loss_rate = round((lost_messages/expected_messages)*100, 4)
                second_wise_plr += [packet_loss_rate]
                count = 0
                tmp = info[l]["msg_rate"]

                # For Dropped by all
                lost_messages = len(dropped_by_all)
                if lost_messages > 0:
                    print(dropped_by_all[0], (int(dropped_by_all[-1]) - int(dropped_by_all[0])) +1, lost_messages)
                packet_loss_rate = round((lost_messages/expected_messages)*100, 4)
                second_wise_plr_lost_by_all += [packet_loss_rate]
                dropped_by_all = []

        drops_rate[l] = {
            "50.0": np.percentile(second_wise_plr, 50.0),
            "90.0": np.percentile(second_wise_plr, 90.0),
            "95.0": np.percentile(second_wise_plr, 95.0),
            "99.0": np.percentile(second_wise_plr, 99.0),
        }

        plt.figure()
        plt.plot(second_wise_plr, label="For Messages Lost By Atleast 1 Receivers")
        plt.plot(second_wise_plr_lost_by_all, label="For Messages Lost By All Receivers")
        plt.legend()
        plt.title(f"Message Rate: {info[l]['msg_rate']}")
        plt.xlabel("Nth second")
        plt.ylabel("Packet Loss Rate (%)")
        # plt.ylim([-1, 30])
        plt.savefig(f"{util.FIGS_DIR}/{l}.pdf")
        print(len(second_wise_plr))

    drops_rate = json.dumps(drops_rate, indent=4)
    print(drops_rate)


def show_drops():
    print("Reads csv files that have been pulled and copied in the assets dir.")
    data_files = util.get_data_files()
    messages = {}
    max_count = 0
    for file in data_files:
        data = util.read_and_deserialize_data(file, clean=True)
        for row in data:
            msg_id = int(row[0])
            if msg_id not in messages:
                messages[msg_id] = 0
            
            messages[msg_id] += 1
            max_count = max(max_count, messages[msg_id])
    
    lost_multicast_messages = 0
    for id in messages:
        if messages[id] < max_count:
            lost_multicast_messages += 1
    
    print("Total: ", len((messages.keys())))
    print("Lost: ", lost_multicast_messages)
    print("Lost Percentage: ", (100*lost_multicast_messages) / len((messages.keys())))


def plot_trading_results():
    pass

    file = "./trading_res/trading_results_jasper.csv"
    df = pd.read_csv(file)

    # Group data by timestamp
    grouped = df.groupby('Time')

    # Create lists to store data for each recipient
    timestamps = []
    recipient_0_shares = []
    recipient_1_shares = []
    recipient_98_shares = []
    recipient_99_shares = []

    # Iterate through each timestamp group
    for timestamp, group in grouped:
        total_balance = group['Balance'].sum()
        
        # Calculate the share of total balance for each recipient
        recipient_0_balance = group[group['Recipientid'] == 0]['Balance'].sum()
        recipient_1_balance = group[group['Recipientid'] == 1]['Balance'].sum()
        recipient_98_balance = group[group['Recipientid'] == 98]['Balance'].sum()
        recipient_99_balance = group[group['Recipientid'] == 99]['Balance'].sum()
        
        recipient_0_share = (recipient_0_balance / total_balance) * 100
        recipient_1_share = (recipient_1_balance / total_balance) * 100
        recipient_98_share = (recipient_98_balance / total_balance) * 100
        recipient_99_share = (recipient_99_balance / total_balance) * 100
        
        # Append data to the lists
        timestamps.append(timestamp)
        recipient_0_shares.append(recipient_0_share)
        recipient_1_shares.append(recipient_1_share)
        recipient_98_shares.append(recipient_98_share)
        recipient_99_shares.append(recipient_99_share)

    # Create a plot for each recipient's share
    plt.figure(figsize=(10, 6))
    plt.plot(timestamps, recipient_0_shares, label='Recipient 0')
    plt.plot(timestamps, recipient_1_shares, label='Recipient 1')
    plt.plot(timestamps, recipient_98_shares, label='Recipient 98')
    plt.plot(timestamps, recipient_99_shares, label='Recipient 99')

    plt.xlabel('Time')
    plt.ylabel('Shares (%)')
    plt.ylim(0, 100)  # Set the y-axis limits from 0 to 100
    plt.legend()
    plt.title('Recipient Shares of Total Balance Over Time')
    plt.grid(True)
    plt.savefig("tmp.pdf")


if __name__ == "__main__":
    mode = str(util.sys.argv[1])
    data_source = str(util.sys.argv[2])
    prefix = str(util.sys.argv[3])
    util.PREFIX = prefix

    if mode == "data":
        main_data()
    elif mode == "alt":
        alt_data()
    elif mode == "loss_exp":
        # loss_data()
        losses.loss_data_per_minute(prefix)
    elif mode == "lp" or mode == "latency_plots":
        latency_plots()
    elif mode == "pi" or mode == "percentage_improvement":
        percentage_improvement_plots()
    elif mode == "prla" or mode == "per_receiver_latency_abnormality":
        main_show_per_receiver_latency_abnormality()
    elif mode == "tsp" or mode == "time_series_plots":
        time_series_plots()
    elif mode == "dr" or mode == "drops_rate":
        # plot_drop_rate()
        show_drops()
    elif mode == "tr" or mode == "trading":
        plot_trading_results()
    elif mode == "sp" or mode == "simple_plots":
        plot_files_x_y()
    elif mode == "cld" or mode == "cumulative_latency_data":
        cumulative_latency_data()
    elif mode == "clp" or mode == "cumulative_latency_plot":
        cumulative_latency_plot()
    else:
        util.print_info("Argument needed: remote/local/lp/prl")
