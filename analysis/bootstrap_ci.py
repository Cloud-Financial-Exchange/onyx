import numpy as np
import matplotlib.pyplot as plt

def get_bootstrap_ci(owds, percentiles_of_interest, bootstrap_samples):
    # List to store the bootstrapped percentiles
    bootstrapped_percentiles = []

    # Perform bootstrapping
    for _ in range(bootstrap_samples):
        # Resample data with replacement
        resampled_data = np.random.choice(owds, size=len(owds), replace=True)
        
        # Calculate percentiles for the resampled data
        percentiles = np.percentile(resampled_data, percentiles_of_interest)
        
        # Store the percentiles in the list
        bootstrapped_percentiles.append(percentiles)

    # Convert the list of percentiles to a NumPy array for easy manipulation
    bootstrapped_percentiles = np.array(bootstrapped_percentiles)

    # Calculate the confidence interval for each percentile
    confidence_interval_lower = np.percentile(bootstrapped_percentiles, 2.5, axis=0)
    confidence_interval_upper = np.percentile(bootstrapped_percentiles, 97.5, axis=0)

    return confidence_interval_lower, confidence_interval_upper
