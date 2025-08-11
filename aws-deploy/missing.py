import json

with open("./../config/ec2-ips.json") as f:
    ips = json.load(f)

with open("./missing.txt") as f:
    data = f.read()

for ip in ips["client"]:
    if ip not in data:
        print("Not found: ", ip)

for ip in ips["recipient"]:
    if ip not in data:
        print("Not found: ", ip)

for ip in ips["proxy"]:
    if ip not in data:
        print("Not found: ", ip)

