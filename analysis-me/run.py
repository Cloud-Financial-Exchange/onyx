import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys

# Load the CSV data into a pandas DataFrame
results_loc = './figs/'

filename = str(sys.argv[1])

print('Analyzing ', filename)

df = pd.read_csv(filename, header=None, names=['timestamp', 'latency'])

timestamps = df['timestamp']

highest_timestamp = timestamps.max()
lowest_timestamp = timestamps.min()
time_duration_microseconds = highest_timestamp - lowest_timestamp
time_duration_seconds = time_duration_microseconds / 1_000_000  # 1 second = 1,000,000 microseconds
num_data_points = len(df)
throughput = num_data_points / time_duration_seconds

print("Highest Timestamp:", highest_timestamp)
print("Lowest Timestamp:", lowest_timestamp)
print("Time Duration (seconds):", time_duration_seconds)
print("Number of Data Points:", num_data_points)
print("Throughput (events per second):", throughput)

# Extract the latencies
latencies = df['latency']

percentiles = list(range(0, 100))

# Plot CDF
plt.figure(figsize=(8, 6))
plt.plot(np.percentile(latencies, percentiles), percentiles, marker='.', linestyle='-')
plt.xlabel('Order Latency (microseconds)')
plt.ylabel('CDF')
plt.title(f'Acheived ME Goodput={int(throughput)} orders/second \n Median Latency: {int(np.percentile(latencies, 50.0))} micros')
plt.grid(True)
plt.savefig(results_loc + filename.split('/')[-1] + '.pdf')

