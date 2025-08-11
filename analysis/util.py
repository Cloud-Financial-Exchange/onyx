from concurrent.futures import ProcessPoolExecutor
from enum import Enum
import json
import sys
import os
import numpy as np
import s3
import bootstrap_ci
import tarfile

PREFIX = ""
LOCAL_ASSETS_DIR = "./assets/"
DATA_DIR = "./data/"
FIGS_DIR = "./figs/test/"
PRINT_INFO = True

class Graph(Enum):
    CDF = 1
    PERCENTAGE_IMPROVEMENT_PER_PERCENTILE = 2
    CDF_PERCENTAGE_IMPROVEMENT_PER_OWD = 3
    TIME_SERIES = 4
    TIME_SERIES_VARIANCE = 5

MARKERS = ["*", ".", "+", "1", '2', 'v', '<', '>', '^', 'h', '3', '4', '8', 's', 'p', 'P', ',', 'H', 'o', 'x', 'D', 'd']
COLORS = ['black', 'orange', 'red', 'magenta', 'brown', 'magenta', 'orange', 'brown', 'cyan', 'magenta', 'gray', 'yellow', 'white', 'olive', 'maroon', 'orange', 'teal', 'lime', 'aqua', 'fuchsia', 'silver', 'gold', 'indigo', 'violet', 'coral', 'salmon', 'peru', 'orchid', 'lavender', 'turquoise']
LINESTYLES = ['-', '--', ':', '-.']

def get_downloaded_filenames():
    fns = [os.path.join(LOCAL_ASSETS_DIR, f) for f in os.listdir(LOCAL_ASSETS_DIR) if os.path.isfile(os.path.join(LOCAL_ASSETS_DIR, f))]
    print(fns)
    return fns


def get_data_files():
    data_source = "local"
    if (len(sys.argv) < 3):
        print("Provide data source (local/remote) as second cmd arg")
        exit(1)

    data_source = str(sys.argv[2])

    data_files = []
    if (data_source == "remote"):
        data_files = s3.download_s3_data_and_get_downloaded_filenames(PREFIX, -1, LOCAL_ASSETS_DIR, get_downloaded_filenames())
    elif (data_source == "local"):
        data_files = get_downloaded_filenames()
    
    data_files = sorted(data_files)
    return data_files


def get_label():
    return PREFIX

def print_info(info):
    if PRINT_INFO:
        print("\033[92m[INFO]: ", info, "\033[0m")

def untar_file(tar_file, extract_path):
    extracted_files = []

    with tarfile.open(tar_file, 'r') as tar:
        for member in tar.getmembers():
            extracted_path = os.path.join(extract_path, member.name)
            tar.extract(member, path=extract_path)
            extracted_files.append(extracted_path)

    return extracted_files

def read_and_deserialize_data(filename: str, name:bool=False, clean=False):
    print_info(f"Reading {filename}")

    if filename.endswith(".tar.gz"):
        files = untar_file(filename, "./assets/")
        assert(len(files) == 1)
        if clean:
            os.remove(filename)
        filename = files[0]

    data = np.loadtxt(filename, delimiter=',', dtype=int, comments='HEADER')
    if clean:
        os.remove(filename)

    print_info(f"Done reading {filename}")
    if name:
        return data, filename
    else:
        return data


def sort_one_reorder_the_other(vec1, vec2):
    paired_data = list(zip(vec1, vec2))
    sorted_data = sorted(paired_data, key=lambda x: x[0])
    sorted_vec1, reordered_vec2 = zip(*sorted_data)
    return sorted_vec1, reordered_vec2


def aggregate_time_series(vec1, percentile, aggregation_step, do_variance=False):
    res_vec1 = []
    res_vec2 = []

    tmp1 = []
    tmp2 = 0
    count = 0
    for ind, ele in enumerate(vec1):
        tmp1 += [ele]
        count += 1

        if (count >= aggregation_step) or ind == len(vec1)-1:
            res = np.percentile(tmp1, percentile)
            if do_variance:
                res = round(np.std(tmp1), 1)
            res_vec1 += [res]
            res_vec2 += [tmp2]
            tmp2 += 1

            tmp1 = []
            count = 0
    return res_vec1, res_vec2