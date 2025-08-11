#!/bin/bash

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <filename> <ip_address>"
  exit 1
fi

filename=$1
ip_address=$2

if [ ! -f "$filename" ]; then
  echo "Error: File '$filename' not found"
  exit 1
fi

# Read the contents of the file into a variable
file_contents=$(cat "$filename")

# Replace all occurrences of the IP_ADDR with the new IP address
file_contents=${file_contents//IP_ADDR/$ip_address}

# Write the modified contents back to the file
echo "$file_contents" > "$filename"

# Now replace the ttcs agent's original config with our new config
sudo cp $filename /etc/opt/ttcs/ttcs-agent.cfg

# Stop the chronyd
sudo systemctl stop chronyd

# Now start the agent
sudo systemctl stop ttcs-agent
sudo systemctl start ttcs-agent
