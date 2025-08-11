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

# Initialize lists to hold tumbling averages
tumbling_avgs1 = tumbling_avgs2 = None

# Calculate tumbling averages for the files provided
if len(filenames) > 0:
    tumbling_avgs1 = calculate_tumbling_averages(filenames[0])

if len(filenames) > 1:
    tumbling_avgs2 = calculate_tumbling_averages(filenames[1])

# Plot the percentage difference if both datasets are available
if tumbling_avgs1 is not None and tumbling_avgs2 is not None:
    # Ensure both arrays are of the same length by truncating the longer one
    min_length = min(len(tumbling_avgs1), len(tumbling_avgs2))
    tumbling_avgs1 = tumbling_avgs1[:min_length]
    tumbling_avgs2 = tumbling_avgs2[:min_length]

    # Calculate the percentage difference
    percentage_diff = 100 * (tumbling_avgs2 - tumbling_avgs1) / tumbling_avgs1

    # Create a corresponding time index for the tumbling windows
    time_index = np.arange(0, len(percentage_diff))

    # Plot the percentage difference
    plt.figure(figsize=(10, 6))
    plt.plot(time_index, percentage_diff, marker='o', label='Percentage Difference (file2 vs. file1)')
    plt.title('Percentage Difference Between Tumbling Window Averages')
    plt.xlabel('Window Index')
    plt.ylabel('Percentage Difference (%)')
    plt.legend()

    # Save the figure
    res = "figs/percentage_difference.pdf"
    plt.savefig(res)
    print("Saved ", res)
else:
    print("Both files are required to calculate percentage difference.")

#python3 diffqsize.py assets/fancy_qlogs_sep10_221.csv assets/simple_qlogs_sep10_218.csv