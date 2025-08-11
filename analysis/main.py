import argparse
import subprocess
import os
import shutil
import tarfile
import csv
import json

import boto3

import ipdb
# ipdb.set_trace()

class MsgData():
    def __init__(self, arr:list):
        if len(arr) != 6:
            raise RuntimeError("Receiver Data has wrong format!")

        self.id         = int(arr[0])
        self.latency    = int(arr[1])
        self.recvid     = int(arr[2])
        self.holding    = int(arr[3])
        self.release    = int(arr[4])
        self.deadline   = int(arr[5])

class ReceiverData():
    def read_csv(self, f:str):
        arr = []
        with open(f, newline='') as file:
            reader = csv.reader(file)
            for _, row in enumerate(reader):
                if len(row) > 0 and not ("HEADER" in row[0]):
                    arr.append(MsgData(row))

            return arr

    def __init__(self, f:str, n:int):
        # print(f"Reading: {f}")
        self.data = self.read_csv(f)
        self.id = self.data[0].recvid
        self.n = n


def run(command:str, verbose:bool=False) -> list[str]:
    ret = subprocess.run(command.split(" "), stdout=subprocess.PIPE, text=True)
    arr = ret.stdout.strip().split('\n')
    if verbose:
        for a in arr:
            print(a)

    return arr

def pull(prefix:str, cloud:str="gcp"):
    print(f"PULLING: {prefix}")
    def awspull(prefix:str):
        s3 = boto3.client('s3')
        objs = s3.list_objects(Bucket="expresults", Prefix=prefix).get('Contents', [])
        if len(objs) <= 1: 
            raise RuntimeError(f"No bucket found on AWS matching prefix: {prefix}")
        else:
            dir = objs[0]['Key'].split("/")[0]
            if os.path.isdir(os.path.join(os.getcwd(), "assets", dir)):
                print(f"Bucket {dir} already pulled down!")
            else:
                os.makedirs(f"assets/{dir}", exist_ok=True)
                for o in objs: 
                    k = o['Key']
                    path = f"assets/{o['Key']}"
                    s3.download_file("expresults", k, path)
                    print(f"Downloaded: {path}")

            return dir
        
    def gpull(prefix:str):
        arr = run("gcloud storage ls gs://exp-results-nyu-systems-multicast/")
        for a in arr:
            if prefix in a:
                dir = a.split("/")[-2]
                if os.path.isdir(os.path.join(os.getcwd(), "assets", dir)):
                    print(f"Bucket {dir} already pulled down!")
                else:
                    run(f"gcloud storage cp --recursive gs://exp-results-nyu-systems-multicast/{dir}* assets")
                return dir
        raise RuntimeError(f"No bucket found on GCP matching prefix: {prefix}")


    def extract(src:str):
        for f in os.listdir(src):
            if f.endswith(".tar.gz"):
                with tarfile.open(os.path.join(src, f)) as tar:
                    tar.extractall(path=src)
                    print(f"Decompressed: {f}")

    if cloud == "gcp":
        dir = gpull(prefix)
    else:
        dir = awspull(prefix)

    extract(f"./assets/{dir}")

def process(prefix:str):
    print(f"PROCESSING: {prefix}")
    def getdir(src:str, match:str):
        for f in os.listdir(src):
            dir = os.path.join(src, f)
            if os.path.isdir(dir):
                if match in f:
                    return dir
        raise RuntimeError(f"No directory found matching prefix: {prefix}")

    def getfiles(src:str):
        arr = []
        for f in os.listdir(src):
            file = os.path.join(src, f)
            if (file.endswith(".csv")):
                arr.append(file)

        return arr

    def summarize(rdata:ReceiverData, recvs:dict, msgs:dict):
        n = len(rdata.data)
        cnt = 0

        for msgdata in rdata.data:
            if not (msgdata.id in msgs.keys()):
                msgs[msgdata.id] = {
                    "unfair_percentage": 0,
                    "unfair": 0,
                    "total": rdata.n,
                }

            # unfair delivery
            if msgdata.holding == 0:
                cnt += 1
                msgs[msgdata.id]["unfair"] += 1
                msgs[msgdata.id]["unfair_percentage"] = ( 
                    (msgs[msgdata.id]["unfair"]
                     /
                     msgs[msgdata.id]["total"]) * 100 
                )

        recvs[rdata.id] = {
                "unfair_percentage": ( (cnt/n) * 100 ),
                "unfair": cnt,
                "total": n,
                "id": rdata.id
        }

    def write(name:str, data:dict):
        with open(f"{name}.json", "w") as f:
            json.dump(data, f)

    dir = getdir("assets", prefix)
    files = getfiles(dir)

    recvs = {}
    msgs = {}

    n = len(files)
    for f in files: 
        rdata = ReceiverData(f, n)
        summarize(rdata, recvs, msgs)
        print(f"Processed: {f}")

    ret = {
        "receivers": recvs,
        "msgs": msgs,
    }

    write(prefix,ret)

def read(prefix:str):
    print(f"READING: {prefix}")
    def sort(d:dict, i:int, s:str):
        return sorted(d.items(), key=lambda x: x[i][s], reverse=True)

    with open(f"{prefix}.json", "r") as f:
        data = json.load(f)

        recvs = data["receivers"]
        msgs = data["msgs"]

        # for r, i in recvs.items():
        #     print(i)

        cnt = 0
        total = len(msgs)
        for _, i in msgs.items():
            if i["unfair"] > 0: 
                cnt += 1
        
        print(f"Unfair Messages: {(cnt/total) * 100}% | {cnt}/{total}")
        for r, i in sort(recvs, 1, "unfair_percentage"):
            print(f"Receiver {r}: Percentage = {i['unfair_percentage']}%")

def copy(prefix:str):
    print(f"COPYING: {prefix}")
    def getdir(src:str, match:str):
        for f in os.listdir(src):
            dir = os.path.join(src, f)
            if os.path.isdir(dir):
                if match in f:
                    return dir
        raise RuntimeError(f"No directory found matching prefix: {prefix}")


    os.system("rm -fv assets/*.csv*")
    dir = getdir("assets", prefix)
    for f in os.listdir(dir):
        if f.endswith("tar.gz"):
            src = os.path.join(dir, f)
            dst = os.path.join("assets", f)
            print(f"Copied: {shutil.copy(src, dst)}")


def parse():
    parser = argparse.ArgumentParser(
        description='Script to automate GCP data analysis.',
        epilog='Example: main.py --mode/-m pull -p <prefix>'
    )

    parser.add_argument(
        "-m", "--mode",
        type=str,
        choices=["pull", "process", "read", "full", "copy"],
        required=True,
        dest="mode",
    )

    parser.add_argument(
        "-c", "--cloud",
        type=str,
        choices=["gcp", "aws"],
        required=False,
        default="gcp",
        dest="cloud",
    )

    parser.add_argument(
        "-p", "--prefix",
        type=str,
        required=True,
        dest="prefix",
    )

    args = parser.parse_args()
    return args

if __name__ == "__main__":
    args = parse()

    if args.mode == "pull": 
        pull(args.prefix, args.cloud)

    elif args.mode == "process": 
        process(args.prefix)

    elif args.mode == "read": 
        read(args.prefix)

    elif args.mode == "full": 
        pull(args.prefix, args.cloud)
        process(args.prefix)
        read(args.prefix)

    elif args.mode == "copy": 
        copy(args.prefix)

    else:
        raise NotImplementedError(f"Mode: {args.mode} not implemented!")

