import json
import matplotlib
import matplotlib.pyplot as plt

matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

LINE_WIDTH = 2
FONT_SIZE = 12  # 14

# Read data from fancyvsimple.json
with open('data/loqvfifo.json', 'r') as file:
# with open('data/fancyvsimple.json', 'r') as file:
    data = json.load(file)

# Extract data for calculation
book_depth = data["Book Depth"]
overall_throughput_fancy_pq = data["OverallThroughput"]["FancyPQ"]
overall_throughput_simple_pq = data["OverallThroughput"]["SimplePQ"]
bursts_throughput_fancy_pq = data["BurstsThroughput"]["FancyPQ"]
bursts_throughput_simple_pq = data["BurstsThroughput"]["SimplePQ"]

# Calculate percentage throughput increase due to FancyPQ
percentage_increase_overall = [
    (100 * (thpt_fancy_pq - thpt_simple_pq) / thpt_simple_pq) 
    for thpt_fancy_pq, thpt_simple_pq in zip(overall_throughput_fancy_pq, overall_throughput_simple_pq)]

percentage_increase_bursts = [
    (100 * (thpt_fancy_pq - thpt_simple_pq) / thpt_simple_pq) 
    for thpt_fancy_pq, thpt_simple_pq in zip(bursts_throughput_fancy_pq, bursts_throughput_simple_pq)]

price_levels_ratio = [1.0/x for x in book_depth]

# Calculate latency decrease due to FancyPQ
overall_latency_fancy_pq = data["OverallLatency"]["FancyPQ"]
overall_latency_simple_pq = data["OverallLatency"]["SimplePQ"]
bursts_latency_fancy_pq = data["BurstsLatency"]["FancyPQ"]
bursts_latency_simple_pq = data["BurstsLatency"]["SimplePQ"]

percentage_decrease_overall = [
    (100 * (thpt_simple_pq - thpt_fancy_pq) / thpt_simple_pq) 
    for thpt_fancy_pq, thpt_simple_pq in zip(overall_latency_fancy_pq, overall_latency_simple_pq)]

percentage_decrease_bursts = [
    (100 * (thpt_simple_pq - thpt_fancy_pq) / thpt_simple_pq) 
    for thpt_fancy_pq, thpt_simple_pq in zip(bursts_latency_fancy_pq, bursts_latency_simple_pq)]

# Plotting
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(4, 4))

# Plot percentage throughput increase due to FancyPQ
ax1.plot(price_levels_ratio, percentage_increase_overall, marker='o', color='b', linestyle="-", label="Overall", linewidth=LINE_WIDTH, markersize=7)
ax1.plot(price_levels_ratio, percentage_increase_bursts, marker='*', color='g', linestyle="--", label="During Bursts", linewidth=LINE_WIDTH, markersize=7)

# ax1.set_xlabel('Ratio of overlapping price levels \nto all the levels', fontsize=FONT_SIZE)
ax1.set_ylabel('%-age Increase In\nME Throughput', fontsize=FONT_SIZE)
ax1.set_xscale('log')
ax1.set_yticklabels([int(x) for x in ax1.get_yticks()], rotation=70)
ax1.set_xticks(price_levels_ratio)
ax1.set_xticklabels(["" for l in book_depth], fontsize=FONT_SIZE)
# ax1.set_xticklabels([r'$\dfrac{1}{'+str(l)+'}$' for l in book_depth], fontsize=FONT_SIZE)
ax1.invert_xaxis()
# ax1.legend(fontsize=FONT_SIZE)
ax1.grid(True)

# Plot percentage latency decrease due to FancyPQ
ax2.plot(price_levels_ratio, percentage_decrease_overall, marker='o', color='b', linestyle="-", label="Overall", linewidth=LINE_WIDTH, markersize=7)
ax2.plot(price_levels_ratio, percentage_decrease_bursts, marker='*', color='g', linestyle="--", label="During Bursts", linewidth=LINE_WIDTH, markersize=7)

ax2.set_xlabel('Ratio of overlapping price levels to all the levels', fontsize=9)
ax2.set_ylabel('%-age Decrease In \nMedian Latency', fontsize=FONT_SIZE)
ax2.set_xscale('log')
min_y1 = min(percentage_decrease_overall)
min_y2 = min(percentage_decrease_bursts)
min_y = min(min_y1, min_y2)
min_y = max(min_y-20, -20)
ax2.set_ylim([min_y, 110])
ax2.set_xticks(price_levels_ratio)
ax2.set_xticklabels([r' $\dfrac{1}{'+str(l)+'}$' for l in book_depth], fontsize=FONT_SIZE)
ax2.set_yticklabels([int(x) for x in ax2.get_yticks()], rotation=70)
ax2.invert_xaxis()
ax2.legend(fontsize=FONT_SIZE, loc='lower right')
ax2.grid(True)

plt.tight_layout()
plt.savefig("figs/loqvfifo_summary.pdf")
# plt.show()
