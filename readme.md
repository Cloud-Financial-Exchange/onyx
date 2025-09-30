## For SIGCOMM Artifact Evaluation

Onyx main contributions include low latency multicast and an LOQ which handles bursts effectively. 
We perform experiments to validate these two points and record our screen while performing the experiments.

### Figure 9 replication (LOQ vs SimplePQ)
Screen-recording of performing LOQ vs SimplePQ experiment: https://drive.google.com/file/d/10Dj5JegVj_nPwQDfX3M13ls0BzEv7Kj2/view?usp=sharing

I first run order submission for SimplePQ. Then I destroy the stack, change the config.h to use LOQ and then re-deploy and run order submission again. 

Then I analyze the two files created from the two experiments to create a plot.

### Figure 5 replication (Onyx vs Direct Unicasts)

We omit a comparison with AWS TGW for this figure because of time constraints. However Onyx is still compared with DU to validate its effectivess. 

Video link: https://drive.google.com/file/d/1ctsZgqm-8bem-u16BaVtk99dNlsIeU3x/view?usp=sharing

### Figure 6 replication (Onyx DWS vs CloudEx DWS)

Following video shows Onyx's comparison with CloudEx (and Onyx H = 0). The plots look different from the ones in the paper, but overall claims still stand: Onyx H=2 performs better than CloudEx and Onyx H = 0. The difference in the plots perhaps can be attributed to transiet change in the underlying cloud fabric's conditions which are not under our control, but in any case the effectiveness of Onyx still holds. 

Video Link: https://drive.google.com/file/d/1RB3jDTvm8jSXp6OieXGXUaoV_spPhmhC/view?usp=drive_link


### Quick guide for other figures replication
To replicate figures other than the above (5, 6, 9), you would need access to a cloud platform with enough balance to perform the experiments. The balance required for an experiment can be calculated by the number of VMs required for each experiment as that is the only major cost. 

Onyx requires constructing a tree so appropriate branching factor needs to be passed to the `create-template` command (more info in Usage section below), also shown in the above videos. For 1000 receivers, `create-template` command would need the ```-p 111 -b 10``` parameters (i.e., 111 proxies are needed and a tree of branching factor 10 needs to be constructed). For 100 receivers, it is ```-p 11 -b 10```. 

Once a template is created, it is deployed to a cloud platform using the `deploy-stack` command. Once all VMs have been deployed, ssh into the client VM (it will have `client` in the name) and perform submit requests for running multicast or order submission. 

The src/config.h has all the configurations that can be used to tune the experiments i.e., turn on/off the hold-and-release or hedging or configure the message rates. This configuration file should be changed before creating and deploying the VMs. 

`src/multicast_client/multicast_client.cpp` has all the options for any commands and can be visualized by the running the client with `-h` option. These options show how to run multicast experiment or order submission experiment or how to store all the statistics to a remote storage (e.g., GCS or S3). 

Once all the statistics are stored in a remote storage, the analysis scripts in the `analysis/` directory are used to plot figures. Running `analysis/run.py` with `-h` shows all the options. 


## Usage

#### On AWS

1. Use aws-deploy/run.py script to generate a CloudFormation template using create_template argument to the script. 

    For example, we want to have 10 receivers, 3 proxies, 1 client, with a branching factor of 2, utilize configuration 0. Then we run:
```python3 run.py create-template -r 10 -p 3 -b 2 -c 1 -rm dpdk -dm large -conf 0```.
2. Use ``python3 run.py deploy-stack -dm large `` which will use the CloudFormation templates generated in the previous step. The script will also bundle the project into a `.tar` and upload it to S3, which will be pulled in each VM in the stack(s).

3. When done, ```python3 run.py delete-stack```.

#### On GCP

##### 1. Generate Cloud Templates:

```bash
cd gcp-deploy
python3 run.py create-template -r 10 -p 3 -b 2 -c 1 -rm dpdk -dm standard -conf 0
```

##### 2. Deploy Cloud Stack:

```bash
cd gcp-deploy # if not already there
python3 run.py deploy-stack -dm standard
```
- It is important to note that the `dm <deploy-mode>` flag must be carried across 
template creation and deployment. If chosen `split` or `standard` during template creation, 
inform that to the stack deployment procedure so it collects the correct templates!

- Look at [src/config.h](src/config.h) before deployment, two variables must be changed across clouds: 
<table>
<tr> <th>AWS</th> <th>GCP</th> </tr>

<tr>
<td>

```cpp 
    const char* S3_STATS_BUCKET = "expresults";
    const char* CLOUD = "aws";
```
</td>
<td>

```cpp 
    const char* S3_STATS_BUCKET = "exp-results-nyu-systems-multicast";
    const char* CLOUD = "gcp";
```
</td>
</tr>

</table> 

- To inspect running instances either search them by name on [VM Instances](https://console.cloud.google.com/compute/instances?referrer=search&project=multicast1) or on the [Deployment Manager](https://console.cloud.google.com/dm/deployments?referrer=search&project=multicast1).
- It is also possible to directly ssh through the terminal via:
```
gcloud compute ssh --zone "us-east4-c" "proxy0000" --tunnel-through-iap --project "multicast1"
```

##### 3. Experiment 
0. On any node/machine, to visualize google's startup-script output run:
```bash
# -o cat: is just an easier on the eyes output to follow
# -f: will show you the latest lines only
journalctl -u google-startup-scripts.service -o cat
```
1. Client/Experiment Run
```bash
sudo su 
cd /home/ubunutu/dom-tenant-service/src/
./build/multicast_client -a mac -t 10
./build/multicast_client -a messages -t 10 -i 0 -s test
```
2. Analysis
``` bash
cd gcp-deploy
# python3 -m <pull,copy,read,full,process> -c <gcp,aws> -p <prefix>
python3 -m pull -c gcp -p cool_prefix_name_on_bucket # pulls down data into a folder within assets/<prefix>
python3 -m copy -c gcp -p cool_prefix_name_on_bucket # copys said data into assets top level
python3 data local                                   # regular script to create json data file
python3 lp local                                     # latency plots
```

##### 4. Delete Cloud Stack:

```bash
cd gcp-deploy # if not already there
python3 run.py delete-stack
```

Notes:

1. If Stack was built using DPDK mode:

    a. On the client run (in src dir)
    `./build/multicast_client -a mac -t 10 -i 0 -s test`. 
    It will run the client in dpdk mode which collects and distributes all the MAC addresses.

    b. Then  run `./build/multicast_client -a messages -t 10 -i 0 -s test` for doing the measurements. -t shows the duration (in seconds) for the experiment, -s shows the prefix for the directory in which results would be stored in aws s3. -i shows the starting message id for the messages. Use 0 starting message id when runnning for the first time. 

## Code Architecture

All roles (client, proxy, receiver) are consisted of 2 parts: **controller** and **worker** threads. Controller is responsible for the Management Message (through TCP connection), while worker is responsible for the actual work related for example multicasting and order submission.

- Client: 
    - Controller: Currently it only collects MAC addresses from all receivers and proxies then distributes MACs to proxies by establishing a TCP connection to all VMs. *(TODO: Add failure detection)*
    - Client only sends packets to the root proxy in UDP in multicasting.
    - After sending the packet, it requests stats from all the receivers with UDP.

- Proxy:
    - Controller: It waits TCP connection from the client and interact with the client. When client issues MAC collection request to it, it will respond with its own MAC and IPs of its own downstream VMs to get their corresponding MACs.
    - There are three kinds of proxy: iouring, socket, and DPDK. DPDK requires MACs for sending messages to its downstream, which means DPDK proxies' worker thread cannot begin until MACs are received from client.
- Receiver:
    - Controller: Share the same controller as proxies, but does not piggyback MACs of downstream VMs since they are leaves.
    - Receivers accept multicast packets from upstream proxies, and respond if the message asks for stats. *(TODO: Make collect_stat a Management Message)*
