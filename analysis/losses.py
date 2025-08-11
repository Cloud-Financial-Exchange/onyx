import matplotlib as mpl
from matplotlib import pyplot as plt
import numpy as np
import util
from labels import labels, texts, info
from collections import defaultdict

mpl.rcParams['font.size'] = 18  # Change 12 to your desired font size
plt.rcParams['axes.prop_cycle'] = plt.cycler(color=util.COLORS)
mpl.rcParams['pdf.fonttype'] = 42
mpl.rcParams['ps.fonttype'] = 42

import random

'''
rrps is a typo, it is reciever hedging indeed
'''
def show_rrps_plot(peak_loss_rate_per_client, total_loss_rate_per_client, num_participants, ymax):
    client_ids = list(peak_loss_rate_per_client.keys())
    assert len(client_ids) % 2 == 0, "Number of VMs (clients) must be even"

    shuffled = client_ids[:]
    random.seed(100)
    random.shuffle(shuffled)

    participant_ids = range(num_participants)
    rrps_peak_rates = []
    rrps_total_rates = []

    for i in range(0, len(shuffled), 2):
        c1 = shuffled[i]
        c2 = shuffled[i + 1]
        rrps_peak_rates.append(min(peak_loss_rate_per_client[c1], peak_loss_rate_per_client[c2]))
        rrps_total_rates.append(min(total_loss_rate_per_client[c1], total_loss_rate_per_client[c2]))

    x = range(num_participants)
    plt.figure(figsize=(10, 5))
    plt.bar(x, rrps_peak_rates, width=0.6, label="RH Peak Loss Rate (%)", color="tab:blue")
    plt.plot(x, rrps_total_rates, label="RH Total Loss Rate (%)", color="green", marker='o', linewidth=3)

    plt.xlabel("Participant ID")
    plt.ylabel("Loss Rate (%)")
    plt.title("Receiver Hedging: Loss Rates per Participant (Min of Paired VMs)")
    plt.legend()
    plt.ylim(0, ymax)
    plt.grid(axis='y', linestyle='--', alpha=0.5)
    plt.tight_layout()
    plt.savefig("figs/loss_exp/test_rrps.pdf")
    plt.close()

def show_plot(peak_loss_rate_per_client, total_loss_rate_per_client):
    client_ids = list(peak_loss_rate_per_client.keys())
    peak_rates = [peak_loss_rate_per_client[c] for c in client_ids]
    total_rates = [total_loss_rate_per_client[c] for c in client_ids]

    # Sort by client ID
    client_ids, peak_rates, total_rates = zip(
        *sorted(zip(client_ids, peak_rates, total_rates), key=lambda x: int(x[0]))
    )
    client_ids = list(client_ids)
    peak_rates = list(peak_rates)
    total_rates = list(total_rates)

    x = range(len(client_ids))

    # Determine y-axis upper bound
    ymax = max(peak_rates + total_rates + [0.01]) * 1.2  # Add fallback if all 0

    plt.figure(figsize=(10, 5))
    plt.bar(x, peak_rates, width=0.6, label="Peak Loss Rate (%)", color="tab:blue")
    plt.plot(x, total_rates, label="Total Loss Rate (%)", color="green", marker='o', linewidth=3)

    plt.xlabel("Participant ID")
    plt.ylabel("Loss Rate (%)")
    plt.title("Peak and Total Loss Rates per Client")
    plt.legend()
    plt.ylim(0, ymax)
    plt.grid(axis='y', linestyle='--', alpha=0.5)
    plt.tight_layout()
    plt.savefig("figs/loss_exp/test.pdf")
    plt.close()

    return ymax


def show_combined_plot(peak_loss_rate_per_client, total_loss_rate_per_client, num_participants):
    client_ids = list(peak_loss_rate_per_client.keys())
    peak_rates = [peak_loss_rate_per_client[c] for c in client_ids]
    total_rates = [total_loss_rate_per_client[c] for c in client_ids]

    # Sort by client ID
    client_ids, peak_rates, total_rates = zip(
        *sorted(zip(client_ids, peak_rates, total_rates), key=lambda x: int(x[0]))
    )

    client_ids = list(client_ids)  # <-- This fixes the shuffle error
    peak_rates = list(peak_rates)
    total_rates = list(total_rates)

    # RRPS pairing (min of adjacent VMs)
    shuffled = client_ids[:]
    random.seed(500)
    random.shuffle(shuffled)

    rrps_peak_rates = []
    rrps_total_rates = []
    for i in range(0, len(shuffled), 2):
        c1 = shuffled[i]
        c2 = shuffled[i + 1]
        rrps_peak_rates.append(min(peak_loss_rate_per_client[c1], peak_loss_rate_per_client[c2]))
        rrps_total_rates.append(min(total_loss_rate_per_client[c1], total_loss_rate_per_client[c2]))

    # Determine y-axis limit
    ymax = max(peak_rates + total_rates + rrps_peak_rates + rrps_total_rates + [0.01]) * 1.2

    fig, axs = plt.subplots(1, 2, figsize=(16, 5))

    # Left subplot: per-client plot
    x1 = range(len(client_ids))
    axs[0].bar(x1, peak_rates, width=0.6, label="Peak Loss Rate (%)", color="tab:blue")
    axs[0].plot(x1, total_rates, label="Total Loss Rate (%)", color="green", marker='o', linewidth=3)
    axs[0].set_xlabel("Participant ID")
    axs[0].set_ylabel("Loss Rate (%)")
    # axs[0].set_title("Peak and Total Loss Rates per Client")
    axs[0].legend()
    axs[0].set_ylim(0, ymax)
    axs[0].grid(axis='y', linestyle='--', alpha=0.5)

    # Right subplot: RRPS plot
    x2 = range(num_participants)
    axs[1].bar(x2, rrps_peak_rates, width=0.6, label="Receiver Hedging Peak Loss Rate (%)", color="tab:blue")
    axs[1].plot(x2, rrps_total_rates, label="Receiver Hedging Total Loss Rate (%)", color="green", marker='o', linewidth=3)
    axs[1].set_xlabel("Participant ID")
    # axs[1].set_title("With RRPS")
    axs[1].legend()
    axs[1].set_ylim(0, ymax)
    axs[1].grid(axis='y', linestyle='--', alpha=0.5)

    plt.tight_layout()
    plt.savefig("figs/loss_exp/test_combined.pdf")
    plt.close()

def loss_data_per_minute(label):
    data_files = util.get_data_files()
    msg_rate_per_sec = info[label]["msg_rate"]
    total_seconds = info[label]["total_seconds"]
    num_receivers = info[label]["num_receivers"]

    expected_total_msgs = msg_rate_per_sec * total_seconds

    peak_loss_rate_per_client = {}
    total_loss_rate_per_client = {}

    lost_msgs_set = set()  # Track unique multicast message IDs missed
    total_drops_per_minute_across_clients = defaultdict(int)  # Track overall lost msgs/min

    for filename in data_files:
        drops_per_minute = defaultdict(float)
        data = util.read_and_deserialize_data(filename, False, False)

        if len(data) and not isinstance(data[0], np.ndarray):
            data = [data]
        client = 0
        total_drops = 0

        for row in data:
            print(row)
            client = int(row[0])
            msgid1 = int(row[1])
            msgid2 = int(row[2])
            ts1 = int(row[3])
            ts2 = int(row[4])

            drops = max(0, msgid2 - msgid1 - 1)
            if drops == 0:
                continue

            # Add lost message IDs for this gap
            lost_msgs_set.update(range(msgid1 + 1, msgid2))

            min1 = (ts1 // 1_000_000) // 60
            min2 = (ts2 // 1_000_000) // 60

            total_drops += drops

            if min1 == min2:
                drops_per_minute[min1] += drops
                total_drops_per_minute_across_clients[min1] += drops
            else:
                duration = min2 - min1 + 1
                drops_per_slot = drops / duration
                for m in range(min1, min2 + 1):
                    drops_per_minute[m] += drops_per_slot
                    total_drops_per_minute_across_clients[m] += drops_per_slot

        # Peak loss rate
        if drops_per_minute:
            loss_pct_per_minute = {
                m: (drops / (msg_rate_per_sec * 60)) * 100
                for m, drops in drops_per_minute.items()
            }
            peak = max(loss_pct_per_minute.values())
            peak_loss_rate_per_client[client] = peak
        else:
            peak_loss_rate_per_client[client] = 0.0

        # Total loss rate (based on total_seconds)
        total_loss_rate = (total_drops / expected_total_msgs) * 100 if expected_total_msgs > 0 else 0.0
        total_loss_rate_per_client[client] = total_loss_rate

    # Fill missing client entries with 0.0
    for client_id in range(num_receivers):
        peak_loss_rate_per_client.setdefault(client_id, 0.0)
        total_loss_rate_per_client.setdefault(client_id, 0.0)

    show_combined_plot(peak_loss_rate_per_client, total_loss_rate_per_client, num_receivers // 2)

    print("Using labels.py for:")
    print("msg_rate: ", msg_rate_per_sec)
    print("total_seconds: ", total_seconds)
    print("num_receivers: ", num_receivers)

    # Summary stats
    lost_msgs_pct = (len(lost_msgs_set) * 100) / expected_total_msgs if expected_total_msgs > 0 else 0.0
    print("Total multicast messages lost (at least one receiver missed them):", len(lost_msgs_set))
    print("Total multicast loss percentage:", lost_msgs_pct, "%")

    peak_lost_msgs_per_minute = max(total_drops_per_minute_across_clients.values(), default=0)
    print("Peak lost multicast messages in any 1-minute interval:", int(peak_lost_msgs_per_minute))

    # Percentage of lost messages in peak 1-minute interval
    expected_msgs_per_minute = msg_rate_per_sec * 60
    peak_lost_pct_per_minute = (peak_lost_msgs_per_minute * 100) / expected_msgs_per_minute if expected_msgs_per_minute > 0 else 0.0
    print("Peak lost percentage in any 1-minute interval:", peak_lost_pct_per_minute, "%")
