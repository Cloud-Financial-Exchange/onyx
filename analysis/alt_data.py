import os
import random
import util

from typing import List
import csv

def pair(files:List):
    _rng   = random.Random()
    pairs  = []
    n      = len(files)
    _files = [ f for f in files ]

    def select(pool:List):
        idx = _rng.randint(0, len(pool) - 1)
        ret = pool[idx]
        pool.pop(idx)
        return ret

    for i in range(int(n/2)):
        fst  = select(_files)
        sec  = select(_files)
        tup  = (fst, sec)
        pairs.append(tup)

    assert(len(pairs) > 0), f"Empty pairs list created!"
    return pairs

def resolve_pairs(f1:str, f2:str, i:int):
    file = f"ALT_{i}.csv"
    file = os.path.join(util.LOCAL_ASSETS_DIR, file)
    result = {}
    data1, _f1  = util.read_and_deserialize_data(f1, name=True)
    data2, _f2  = util.read_and_deserialize_data(f2, name=True)

    print(f"FILE1: {_f1}")
    print(f"FILE2: {_f2}")

    for row in data1:
        mid = int(row[0])
        owd = int(row[1])
        rid = int(row[2])

        result[mid] = [mid, owd, rid]

        if len(row) > 3:
            hold = int(row[3])
            result[mid].append(hold)
    
    for row in data2:
        mid = int(row[0])
        owd = int(row[1])
        rid = int(row[2])

        if mid not in result:
            result[mid] = [mid, owd, rid]

            if len(row) > 3:
                hold = int(row[3])
                result[mid].append(hold)
        else:
            ref_owd = result[mid][1]
            if owd < ref_owd:
                result[mid] = [mid, owd, rid]

                if len(row) > 3:
                    hold = int(row[3])
                    result[mid].append(hold)

    os.remove(_f1)
    os.remove(_f2)

    with open(file, 'w', newline='') as f:
        write = csv.writer(f)
        for _, row in result.items():
            write.writerow(row)

    print(f"ALT_FILE: {file}")
    return file

