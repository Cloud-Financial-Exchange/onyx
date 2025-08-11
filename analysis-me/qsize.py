import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys

CORRECTION_PER_VALUE = 0x20202020
# CORRECTION_PER_VALUE = 0

# Get filenames from command line arguments
filenames = sys.argv[1:]

# Define the window size
window_size = 70000

# Function to calculate tumbling averages
def calculate_tumbling_averages(filename):
    df = pd.read_csv(filename, header=None, dtype=float)
    df[0] = df[0] - CORRECTION_PER_VALUE  # Subtract 538976288 from each value
    pad_size = window_size - (len(df) % window_size)
    padded_data = np.pad(df[0].values, (0, pad_size), mode='constant', constant_values=np.nan)
    tumbling_avgs = np.nanmean(padded_data.reshape(-1, window_size), axis=1)
    return tumbling_avgs

# Plot data based on number of files
plt.figure(figsize=(10, 6))

if len(filenames) > 0:
    tumbling_avgs1 = calculate_tumbling_averages(filenames[0])
    time_index1 = np.arange(0, len(tumbling_avgs1))
    plt.plot(time_index1, tumbling_avgs1, marker='o', label=f'{filenames[0]} Tumbling Average (Window = {window_size})')

if len(filenames) > 1:
    tumbling_avgs2 = calculate_tumbling_averages(filenames[1])
    time_index2 = np.arange(0, len(tumbling_avgs2))
    plt.plot(time_index2, tumbling_avgs2, marker='x', label=f'{filenames[1]} Tumbling Average (Window = {window_size})')

# Plot settings
plt.title('Tumbling Window Averages')
plt.xlabel('Window Index')
plt.ylabel('Average Value')
plt.legend()

# Save the figure
res = "figs/qsize_combined.pdf"
plt.savefig(res)
print("Saved ", res)

#python3 qsize.py assets/fancy_qlogs_sep10_221.csv assets/simple_qlogs_sep10_218.csv