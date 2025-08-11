import random
import matplotlib.pyplot as plt
from scipy.stats import mode
import numpy as np
import matplotlib as mpl
mpl.rcParams['font.size'] = 8  # Change 12 to your desired font size

# rv_lower = 0
# rv_upper = 1

# rv_lower = 70
# rv_upper = 30

ls = ['solid', (0, (3, 5, 1, 5, 1, 5)), 'dotted', 'dashed', 'dashdot']

TREE_DEPTH = 3 # Depth 0 means one node
# RV_TYPE = random.uniform
RV_TYPE = random.expovariate

H_UPPER = 6
D_UPPER = 6

INTER_PACKET_DELAY = 15
HISTORY = 2

desired_mean = 60
desired_std_dev = 50
min_value = 40

# Calculate parameters for log-normal distribution
sigma_normal = np.sqrt(np.log(1 + (desired_std_dev / desired_mean)**2))
mean_normal = np.log(desired_mean) - 0.5 * sigma_normal**2

# Adjust mean and standard deviation to approximate minimum value
scale_factor = min_value / np.exp(mean_normal)

# Adjusted mean and standard deviation
adjusted_mean = mean_normal + np.log(scale_factor)
adjusted_std_dev = sigma_normal

def get_rv():
    # return RV_TYPE(rv_lower, rv_upper)

    # mean = 40
    # min_value = 30
    # return RV_TYPE(1/mean) + min_value

    # Obtain draw from log-normal distribution
    return np.random.lognormal(mean=adjusted_mean, sigma=adjusted_std_dev)


def T(d, h):
    if d == 0:
        return 0

    if d == 1:
        return get_rv()

    a = T(d-1, h) + get_rv()

    for i in range(1, h):
        a = min(a, T(d-1, h) + get_rv())

    return round(a, 2)


def T2(d, h):
    a = T(d, h)
    for i in range(1, HISTORY+1):
        a = min(a, (i*INTER_PACKET_DELAY) + T(d, h))
    return a

def simulate_max_distribution(num_simulations, fn, h, d, ls, label):
    res_values = []

    for _ in range(num_simulations):
        res_values.append(fn(d, h))

    mean_value = round(np.mean(res_values), 1)
    std = round(np.std(res_values), 1)

    cdf_values = list(np.percentile(res_values, list(range(0, 100))))

    plt.plot(cdf_values, list(range(0, 100)), linestyle=ls, label=f"H={h-1}, D={d}, {label}, history={HISTORY}")
    # plt.plot(cdf_values, list(range(0, 100)), linestyle=ls, label=f"H={h-1}, D={d}, μ={mean_value}, σ={std}, {label}, history={HISTORY}")

    # # Plot the histogram
    # plt.title("Hedging Analysis")
    # counts, edges, patches = plt.hist(res_values, bins=50, density=True, alpha=0.7, label=f"H={h-1}, D={d}, μ={mean_value}, σ={std}, {label}")

    # plt.axvline(mean_value, color=patches[0].get_facecolor(), linestyle='dashed', linewidth=2)

    plt.xlabel('Random Variable Value')
    plt.ylabel('CDF')


# Set the number of simulations
# num_simulations = 1000
num_simulations = 100000

# Run the simulation and plot the distribution
# plt.figure()
# for h in range(1, H_UPPER):
#     simulate_max_distribution(num_simulations, T, h=h, d=4)
# plt.savefig(f"analysis.pdf")

# plt.figure()
# for d in range(1, D_UPPER):
#     simulate_max_distribution(num_simulations, T, h=1, d=d)
#     print(d)
# plt.savefig(f"analysis2.pdf")

plt.figure()
# plt.tight_layout()
HISTORY = 0
# simulate_max_distribution(num_simulations, T, h=1, d=1, ls=ls[0], label="no coding")
simulate_max_distribution(num_simulations, T, h=2, d=3, ls=ls[0], label="no coding")
simulate_max_distribution(num_simulations, T, h=3, d=3, ls=ls[0], label="no coding")
simulate_max_distribution(num_simulations, T, h=4, d=3, ls=ls[0], label="no coding")

HISTORY = 2
for ipd in range(50, -10, -10):
    INTER_PACKET_DELAY = ipd
    simulate_max_distribution(num_simulations, T2, h=2, d=3, ls=ls[2], label=f"w coding, inter-packet: {INTER_PACKET_DELAY}")

plt.legend(loc='lower right')

plt.savefig(f"test.pdf")