import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys
import os

# Ensure output directory exists
output_dir = "figs"
os.makedirs(output_dir, exist_ok=True)

# Process each file passed as an argument
file_paths = sys.argv[1:]  # Multiple file paths
if not file_paths:
    print("Error: No file paths provided. Usage: python script.py file1.csv file2.csv ...")
    sys.exit(1)

# Create a single plot for all files
plt.figure(figsize=(10, 8))

for file_path in file_paths:
    try:
        # Step 1: Read the CSV file
        data = pd.read_csv(file_path, header=None)  # Assuming no header
        values = data.iloc[:, 0].dropna()  # Extract the single column and drop NaNs

        # Step 2: Calculate percentiles (1 to 100)
        percentiles = np.percentile(values, np.arange(1, 101))

        # Print key percentiles
        print(f"File: {file_path}")
        print("  50p:", np.percentile(values, 50.0))
        print("  90p:", np.percentile(values, 90.0))

        # Step 3: Add CDF to the plot
        plt.plot(
            percentiles, 
            np.arange(1, 101) / 100, 
            marker=".", 
            linestyle="-", 
            label=file_path  # Use file path as the label
        )

    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found. Skipping...")
    except pd.errors.EmptyDataError:
        print(f"Error: File '{file_path}' is empty or not a valid CSV. Skipping...")
    except Exception as e:
        print(f"Error processing file '{file_path}': {e}")

# Step 4: Customize and save the plot
plt.title("CDF of Data from Multiple Files", fontsize=16)
plt.xlabel("Data Values", fontsize=14)
plt.ylabel("Cumulative Probability", fontsize=14)
plt.grid(True)
plt.legend(title="File Paths", fontsize=10)
output_file = os.path.join(output_dir, "0000stress.pdf")
plt.savefig(output_file)
print(f"Combined CDF plot saved to: {output_file}")
plt.close()
