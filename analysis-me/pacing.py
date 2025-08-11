import matplotlib.pyplot as plt

# Function to read and parse the pacing data
def read_pacing_data(file_path):
    data = []
    with open(file_path, 'r') as file:
        lines = file.readlines()
        for line in lines:
            timestamp, value = line.strip().split(',')
            data.append((int(timestamp), int(value)))
    return data

# Function to plot the pacing data with relative seconds on the x-axis
def plot_pacing_data(data, ax):
    # Unzip the data into two lists: timestamps and values
    timestamps, values = zip(*data)
    
    # Convert timestamps to relative seconds
    start_time = timestamps[0]
    relative_seconds = [(t - start_time) / 1e6 for t in timestamps]  # Convert microseconds to seconds
    
    # Plot the data
    ax.plot(relative_seconds, values, linestyle='', marker=".", color='red')