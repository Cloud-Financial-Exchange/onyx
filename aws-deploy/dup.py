import json

with open("./../config/ec2-ips.json") as f:
    ips = json.load(f)

recvs = ips["recipient"]
for i in range(110):
    ips[f"recipient{i}"] = recvs

with open("./../config/ec2-ips.json", "w") as f:
    f.write(json.dumps(ips, indent=4))
