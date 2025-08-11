import numpy as np

# Parameters
rate = 10000  # Rate of XYZ events per second
duration = 20  # Total duration in seconds
event_values = [1, 2, 3]  # Possible values for XYZ events

# Calculate the total number of events expected
total_events = int(rate * duration)

# Generate Poisson distributed event times
event_times = np.random.exponential(scale=1/rate, size=total_events)

# Sort the event times
event_times.sort()

# Initialize an empty list to store the event values
events = []

# Generate random values for each event time
for _ in range(total_events):
    event_value = np.random.choice(event_values)
    events.append((event_times[_], event_value))

# Filter events that occur within the duration
events_within_duration = [(t, v) for t, v in events if t <= duration]

# Display the first few events
print("Time\tValue")
for t, v in events_within_duration[:10]:
    print(f"{t:.4f}\t{v}")
