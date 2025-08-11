import random
import matplotlib.pyplot as plt
import numpy as np
import matplotlib as mpl

mpl.rcParams['pdf.fonttype'] = 42
mpl.rcParams['ps.fonttype'] = 42


# Set font size
mpl.rcParams['font.size'] = 15

# RV bounds and line styles
rv_lower = 0
rv_upper = 1
ls = [(0, (3, 5, 1, 5, 1, 5)), 'dotted', 'dashed', 'dashdot', 'solid']

# Parameters
TREE_DEPTH = 3
RV_TYPE = random.uniform
H_UPPER = 6
D_UPPER = 6

def get_rv():
    return RV_TYPE(rv_lower, rv_upper)

def T(d, h):
    if d == 0:
        return 0
    if d == 1:
        return get_rv()
    a = T(d - 1, h) + get_rv()
    for _ in range(1, h):
        a = min(a, T(d - 1, h) + get_rv())
    return round(a, 2)

# Compute and plot CDF via percentiles
def simulate_max_distribution(num_simulations, fn, h, d, ls):
    samples = np.array([fn(d, h) for _ in range(num_simulations)])
    mean_value = round(np.mean(samples), 1)
    std = round(np.std(samples), 1)

    percentiles = np.arange(1, 101)  # 1st to 100th percentile
    x_vals = np.percentile(samples, percentiles)
    y_vals = percentiles / 100.0     # Convert to [0, 1]

    plt.plot(x_vals, y_vals, label=f"H={h-1}, D={d}, μ={mean_value}, σ={std}",
             linestyle=ls, linewidth=2)

    plt.xlabel('Random Variable Value')
    plt.ylabel('CDF')
    plt.legend()
    plt.grid(True)

# Number of simulations
num_simulations = 100000

# Plot 1: Vary D with H=1
plt.figure()
plt.tight_layout()
simulate_max_distribution(num_simulations, T, h=1, d=1, ls=ls[0])
simulate_max_distribution(num_simulations, T, h=1, d=2, ls=ls[1])
simulate_max_distribution(num_simulations, T, h=1, d=3, ls=ls[2])
simulate_max_distribution(num_simulations, T, h=1, d=4, ls=ls[3])
simulate_max_distribution(num_simulations, T, h=1, d=5, ls=ls[4])
plt.savefig("h1_d_impact.pdf")

# Plot 2: Vary H with D=4
plt.figure()
plt.tight_layout()
simulate_max_distribution(num_simulations, T, h=1, d=4, ls=ls[0])
simulate_max_distribution(num_simulations, T, h=2, d=4, ls=ls[1])
simulate_max_distribution(num_simulations, T, h=3, d=4, ls=ls[2])
simulate_max_distribution(num_simulations, T, h=4, d=4, ls=ls[3])
simulate_max_distribution(num_simulations, T, h=5, d=4, ls=ls[4])
plt.savefig("d4_h_impact.pdf")

# Plot 3: Vary D with H=5
plt.figure()
plt.tight_layout()
simulate_max_distribution(num_simulations, T, h=5, d=1, ls=ls[0])
simulate_max_distribution(num_simulations, T, h=5, d=2, ls=ls[1])
simulate_max_distribution(num_simulations, T, h=5, d=3, ls=ls[2])
simulate_max_distribution(num_simulations, T, h=5, d=4, ls=ls[3])
simulate_max_distribution(num_simulations, T, h=5, d=5, ls=ls[4])
plt.savefig("h4_d_impact.pdf")
