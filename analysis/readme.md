
Quick commands:

python3 main.py -m pull -c gcp -p cool_prefix_name_on_bucket # pulls down data into a folder within assets/<prefix>
python3 main.py -m copy -c gcp -p cool_prefix_name_on_bucket # copys said data into assets top level
python3 run.py data local cool_prefix_name_on_bucket         # regular script to create json data file
python3 run.py lp local cool_prefix_name_on_bucket           # latency plots