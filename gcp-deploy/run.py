import os
import subprocess
import json
import argparse
import yaml
import numpy
import pathlib

# Be mindful of the paths when anything is moved around
script_location = os.path.dirname(os.path.abspath(__file__))
root_dir = os.path.join(script_location, "../")

STACK_NAME = "multicast-service"
STACK_FILE = "build/multicast-service-template.yaml"
SPLIT_STACK_FILE = "build/multicast-service-template-{}.yaml"
BKT_NAME = "cdm-templates-nyu-systems-multicast"

STARTUP_SCRIPT_DIR = "startup-scripts/"
CLIENT_STARTUP_SCRIPT = STARTUP_SCRIPT_DIR + "client.sh"
PROXY_STARTUP_SCRIPT = STARTUP_SCRIPT_DIR + "proxy.sh"
RECIPIENT_STARTUP_SCRIPT = STARTUP_SCRIPT_DIR + "recipient.sh"

deploy_cmd = f"gcloud deployment-manager -q deployments create {STACK_NAME} --config {STACK_FILE}"
deploy_split_cmd = f"gcloud deployment-manager -q deployments create {STACK_NAME}-{{}} --config {SPLIT_STACK_FILE}"
delete_cmd = f"gcloud deployment-manager -q deployments delete {STACK_NAME}"
delete_split_cmd = f"gcloud deployment-manager -q deployments delete {STACK_NAME}-{{}}"


def get_stack_file(number=None):
    if number is None:
        return STACK_FILE
    else:
        return SPLIT_STACK_FILE.format(number)

def get_deploy_cmd(number=None):
    if number is None:
        return deploy_cmd
    else:
        return deploy_split_cmd.format(number, number)

def get_delete_cmd(number=None):
    if number is None:
        return delete_cmd
    else:
        return delete_split_cmd.format(number)


def upload_file(local_file, obj_name):
    subprocess.run(f"gcloud storage cp {local_file} gs://{BKT_NAME}/{obj_name}", shell=True)


def load_file(file_name):
    with open(file_name, "r") as f:
        return str(f.read())


def load_yaml(file_name):
    with open(file_name, "r") as stream:
        return yaml.safe_load(stream)


def write_yaml(file_name, data):
    with open(file_name, "w") as stream:
        yaml.dump(data, stream)


def next_ip(ip, i):
    octets = ip.split(".")
    if int(octets[-1]) + i > 255:
        while (int(octets[-1]) + i > 255):
            octets[-2] = str(int(octets[-2]) + 1)
            i -= 256
    octets[-1] = str(int(octets[-1]) + i)
    return ".".join(octets)


def get_total_redundant_proxies(total_proxies, proxy_tree_branching_factor, redundancy_factor):
    total_internal_nodes = (total_proxies - 1) / proxy_tree_branching_factor
    return int(total_internal_nodes * redundancy_factor)


def write_new_ips_config(ips, base_port, dup_num=1):
    ips["client"] = [f"{x}:{base_port}" for x in ips["client"]]
    ips["client_management"] = [f"{x}:{base_port}" for x in ips["client_management"]]
    ips["proxy"] = [f"{x}:{base_port}" for x in ips["proxy"]]
    ips["proxy_management"] = [f"{x}:{base_port}" for x in ips["proxy_management"]]
    ips["recipient"] = [f"{x}:{int(base_port) + i * 100}" for x in ips["recipient"] for i in range(dup_num)]
    ips["recipient_management"] = [f"{x}:{base_port}" for x in ips["recipient_management"]]
    with open("./../config/ec2-ips.json", "w") as f:
        f.write(json.dumps(ips, indent=4))


def sample_entity(entity, i, total_instances_of_entity,
                  dup_num=1):  # so that I dont have to sync time on every recipeint and proxy
    if entity == "client":
        return True
    if entity == "proxy":
        return True
    if entity == "recipient":  # and (i >= (total_instances_of_entity/dup_num)-48):
        return True
    return False


def make_aws_config():
    with open("./aws_config.json") as f:
        aws_config = json.load(f)
    aws_config["s3_bucket"] = BKT_NAME
    with open("../config/aws_config.json", "w") as f:
        f.write(json.dumps(aws_config, indent=4))


def convert_dict_to_metadata(metadata_dict: dict):
    out = []
    for key, value in metadata_dict.items():
        out.append({"key": key, "value": str(value)})
    return out


def get_metadata(entity, i, ip, run_mode=0, proxy_tree_branching_factor=8, total_instances_of_entity=0, dup_num=1,
                 redundancy_factor=0, rcvr_mode="socket"):
    metadata = {}

    # Startup script https://cloud.google.com/compute/docs/instances/startup-scripts/linux
    if entity == "client":
        metadata["startup-script"] = load_file(CLIENT_STARTUP_SCRIPT)
    if entity == "proxy":
        metadata["startup-script"] = load_file(PROXY_STARTUP_SCRIPT)
    if entity == "recipient":
        metadata["startup-script"] = load_file(RECIPIENT_STARTUP_SCRIPT)

    # VM Attributes
    metadata["IP_ADDR"] = ip
    metadata["INSTANCE_NUM"] = i
    metadata["BKT_NAME"] = BKT_NAME
    metadata["RUN_MODE"] = run_mode
    metadata["CLOUD"] = "gcp"

    if entity == "proxy" or entity == "recipient":
        metadata["BRANCHING_FACTOR"] = proxy_tree_branching_factor
        metadata["REDUNDANCY_FACTOR"] = redundancy_factor
        metadata["TOTAL_NON_REDUNDANT_PROXIES"] = total_instances_of_entity
        metadata["DUPLICATION_FACTOR"] = dup_num

    return convert_dict_to_metadata(metadata)


def get_network_interface(type, private_ip):
    # The GCloud docs recommend using two different VPCs with DPDK. We're going to try using just one "combined"
    network_interface = load_yaml(f"./templates/network-interface-{type}.yaml")
    network_interface["networkIP"] = private_ip
    return network_interface


def get_vm(entity, i, values, new_ips, run_mode=0, proxy_tree_branching_factor=8, total_instances_of_entity=0,
           total_non_redundant_proxies=0, dup_num=1, redundancy_factor=0, rcvr_mode="socket"):
    vm = load_yaml(f"./templates/instance.yaml")
    ip = next_ip(values[f"base_{entity}_vm_ip"], i)
    man_ip = next_ip(values[f"base_{entity}_management_ip"], i)

    vm["name"] = f"{entity}{str(i).zfill(4)}"
    vm["properties"]["networkInterfaces"] = []

    vm["properties"]["metadata"]["items"] = get_metadata(entity, i, man_ip, run_mode, proxy_tree_branching_factor,
                                                         total_non_redundant_proxies, dup_num=dup_num,
                                                         redundancy_factor=redundancy_factor, rcvr_mode=rcvr_mode)

    vm["properties"]["networkInterfaces"].append(get_network_interface("management", man_ip))
    vm["properties"]["networkInterfaces"].append(get_network_interface("service", ip))
    new_ips[f"{entity}"] += [ip]
    new_ips[f"{entity}_management"] += [man_ip]

    return vm


def create_template_from_resources(resources):
    return {"resources": resources}

def make_stack_files(clients, proxies, recipients):
    pathlib.Path("build").mkdir(exist_ok=True)

    # First, make a single file for a standard deployment
    write_yaml(get_stack_file(), create_template_from_resources(clients + proxies + recipients))

    recipient_stacks = list(map(lambda arr: arr.tolist(), numpy.array_split(recipients, 5)))

    # Next, create split stack. Put the service VMs into stack 0
    write_yaml(get_stack_file(0), create_template_from_resources(clients + proxies))
    # Finally, populate stacks 1 to 5 with the recipients
    for i in range(0, 5):
        write_yaml(get_stack_file(i + 1), create_template_from_resources(recipient_stacks[i]))

def show_ip_ranges(ips):
    print("IP Ranges:")
    print("Client:              ", ips["client"][0], " -- ", ips["client"][-1])
    print("Proxy Management:    ", ips["proxy_management"][0], " -- ", ips["proxy_management"][-1])
    print("Proxy:               ", ips["proxy"][0], " -- ", ips["proxy"][-1])
    print("Recipient:           ", ips["recipient"][0], " -- ", ips["recipient"][-1])
    print("Recipient Management:", ips["recipient_management"][0], " -- ", ips["recipient_management"][-1])

def create_template(total_recipients, total_proxies=1, redundancy_factor=1, total_clients=1, run_mode=0,
                    proxy_tree_branching_factor=8, dup_num=1, config_index=0, rcvr_mode="socket"):
    if total_recipients % dup_num:
        print("The number of recipients has to be divisible by duplication factor")
        exit(1)

    clients = []
    proxies = []
    recipients = []

    with open("./template_values.json") as f:
        values = json.load(f)["configs"][config_index]
    print(json.dumps(values, indent=4))

    new_ips = {
        "client": [],
        "client_management": [],
        "proxy": [],
        "proxy_management": [],
        "recipient": [],
        "recipient_management": [],
    }

    # Clients
    for i in range(0, total_clients):
        clients.append(get_vm("client", i, values, new_ips, run_mode, proxy_tree_branching_factor, total_clients))

    # Proxies
    total_redundant_proxies = get_total_redundant_proxies(total_proxies, proxy_tree_branching_factor, redundancy_factor)
    for i in range(0, total_proxies + total_redundant_proxies):
        proxies.append(
            get_vm("proxy", i, values, new_ips, run_mode, proxy_tree_branching_factor, total_proxies, total_proxies,
                   dup_num=dup_num, redundancy_factor=redundancy_factor))

    # Recipient
    for i in range(0, int(total_recipients / dup_num)):
        recipients.append(
            get_vm("recipient", i, values, new_ips, run_mode, proxy_tree_branching_factor, total_recipients,
                   total_proxies, dup_num, redundancy_factor=redundancy_factor, rcvr_mode=rcvr_mode))

    write_new_ips_config(new_ips, values["port"], dup_num)
    make_stack_files(clients, proxies, recipients)
    show_ip_ranges(new_ips)


def create_project_bundle():
    print("Creating bundle of project")
    comm = f"tar -czf {script_location}/bundled_proj.tar.gz --exclude=bundled_proj.tar.gz --exclude analysis --exclude analysis-me/assets --exclude analysis-me/figs --exclude test --exclude .git -C {root_dir} ."
    print(comm)
    subprocess.run(comm, shell=True)

    print("Uploading bundle to bucket")
    upload_file(f"{script_location}/bundled_proj.tar.gz", "bundled_proj.tar.gz")

    print("Uploading startup scripts")
    upload_file(CLIENT_STARTUP_SCRIPT, STARTUP_SCRIPT_DIR + "client.sh")
    upload_file(PROXY_STARTUP_SCRIPT, STARTUP_SCRIPT_DIR + "proxy.sh")
    upload_file(RECIPIENT_STARTUP_SCRIPT, STARTUP_SCRIPT_DIR + "recipient.sh")


def deploy_stack(split=False):
    create_project_bundle()

    # Deploy the stack
    if not split:
        subprocess.run(get_deploy_cmd(), shell=True)
    else:
        for i in range(0, 6):
            subprocess.run(get_deploy_cmd(i), shell=True)

    print("Cleaning up targets")
    subprocess.run(f"rm {script_location}/bundled_proj.tar.gz", shell=True)

def delete_stack(split=False):
    if not split:
        subprocess.run(get_delete_cmd(), shell=True)
    else:
        for i in range(0, 6):
            subprocess.run(get_delete_cmd(i), shell=True)

    print("Cleaning up bucket")
    subprocess.run(f"gsutil -m rm -r gs://{BKT_NAME}/*", shell=True)

def main():
    arg_def = argparse.ArgumentParser()
    arg_def.add_argument(
        "action",
        choices=["deploy-stack", "delete-stack", "create-template"],
        help="Example: python3 run.py create-template -r 100 -p 1 -b 100 -red 0 -c 1 -rm iouring -d 1 -dm large"
    )
    arg_def.add_argument(
        "-r", "--recipient-count",
        type=int,
        default=3,
        dest="recipients",
    )
    arg_def.add_argument(
        "-p", "--proxy-count",
        type=int,
        default=3,
        dest="proxies",
        help="Total number of proxes (without accounting for redundancy factor)"
    )
    arg_def.add_argument(
        "-c", "--client-count",
        type=int,
        default=1,
        dest="clients",
    )
    arg_def.add_argument(
        "-b", "--branching-factor",
        type=int,
        default=8,
        dest="branching",
    )
    arg_def.add_argument(
        "-red", "--redundancy-factor",
        type=int,
        default=0,
        dest="redundancy",
    )
    arg_def.add_argument(
        "-d", "--duplication-factor",
        type=int,
        default=1,
        dest="duplication",
    )
    arg_def.add_argument(
        "-rm", "--run-mode",
        choices=["socket", "dpdk", "iouring"],
        default="socket",
        dest="run_mode",
    )
    arg_def.add_argument(
        "-rcvrm", "--rcvr-mode",
        choices=["socket", "dpdk"],
        default="dpdk",
        dest="rcvr_mode",
    )
    arg_def.add_argument(
        "-dm", "--deploy-mode",
        choices=["standard", "split"],
        default="standard",
        dest="deploy_mode",
    )
    arg_def.add_argument(
        "-conf", "--config-index",
        type=int,
        default=0,
        dest="config_index",
    )

    args = arg_def.parse_args()

    match args.action:
        case "deploy-stack":
            print("Deploy stack")
            match args.deploy_mode:
                case "standard":
                    print("Standard deployment")
                    deploy_stack()
                case "split":
                    print("Split deployment")
                    deploy_stack(split=True)
        case "delete-stack":
            print("Delete stack")
            match args.deploy_mode:
                case "standard":
                    print("Standard deployment")
                    delete_stack()
                case "split":
                    print("Split deployment")
                    delete_stack(split=True)
        case "create-template":
            print("Create template")
            create_template(
                total_recipients=args.recipients,
                total_proxies=args.proxies,
                redundancy_factor=args.redundancy,
                total_clients=args.clients,
                run_mode=args.run_mode,
                proxy_tree_branching_factor=args.branching,
                dup_num=args.duplication,
                config_index=args.config_index,
                rcvr_mode=args.rcvr_mode,
            )

main()
