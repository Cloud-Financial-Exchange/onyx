import sys
import os
import subprocess
import json
import argparse

# Be mindful of the paths when anything is moved around
script_location = os.path.dirname(os.path.abspath(__file__))
root_dir = os.path.join(script_location, "../")

STACK_NAME = "haoTest"
STACK_FILE = "./_stack.json"
BKT_NAME = "cf-templates-hao"
TOTAL_SPLIT_STACK_PIECES = 2

deploy_cmd = f"aws cloudformation deploy --stack-name {STACK_NAME} --template-file {STACK_FILE} --region us-east-2"
deploy_large_cmd = f"aws cloudformation deploy --stack-name {STACK_NAME} --template-file {STACK_FILE} --region us-east-2 --s3-bucket {BKT_NAME}"
delete_cmd = f"aws cloudformation delete-stack --stack-name {STACK_NAME} --region us-east-2"
validate_cmd = f"aws cloudformation validate-template --template-body file://{STACK_FILE[2:]}"

def next_ip(ip, i):
    octets = ip.split(".")
    if int(octets[-1]) +i  > 255:
        while (int(octets[-1]) +i > 255):
            octets[-2] = str(int(octets[-2]) + 1)
            i -= 256
    octets[-1] = str(int(octets[-1])+i)
    return ".".join(octets)

def get_total_redundant_proxies(total_proxies, proxy_tree_branching_factor, redundancy_factor):
    if proxy_tree_branching_factor == 0:
        return 0
    total_internal_nodes = (total_proxies-1)/proxy_tree_branching_factor
    return int(total_internal_nodes * redundancy_factor)

def write_new_ips_config(ips, base_port, dup_num=1):
    ips["client"] = [f"{x}:{base_port}" for x in ips["client"]]
    ips["proxy"] = [f"{x}:{base_port}" for x in ips["proxy"]]
    ips["proxy_management"] = [f"{x}:{base_port}" for x in ips["proxy_management"]]
    ips["recipient_management"] = [f"{x}:{int(base_port) + i * 100}" for x in ips["recipient_management"] for i in range(dup_num)]
    ips["recipient"] = [f"{x}:{int(base_port) + i * 100}" for x in ips["recipient"] for i in range(dup_num)]
    with open("./../config/ec2-ips.json", "w") as f:
        f.write(json.dumps(ips, indent=4))

def sample_entity(entity, i, total_instances_of_entity, dup_num=1): # so that I dont have to sync time on every recipeint and proxy
    if entity == "client":
        return True

    if entity == "proxy":
        return True

    if entity == "recipient": # and (i >= (total_instances_of_entity/dup_num)-48):
        return True

    return False

def make_aws_config():
    with open("./aws_config.json") as f:
        aws_config = json.load(f)
    aws_config["s3_bucket"]=BKT_NAME
    with open("../config/aws_config.json", "w") as f:
        f.write(json.dumps(aws_config, indent=4))

def get_user_data(entity, i, ip, run_mode=0, proxy_tree_branching_factor=8, total_instances_of_entity=0, dup_num=1, redundancy_factor=0, rcvr_mode="socket"):
    if entity != "recipient":
        rcvr_mode = ""

    with open(f"./user_data/{entity}{rcvr_mode}.json") as f:
        user_data = json.load(f)

    instance_num = i
    if entity == "recipient":
        # INSTANCE_NUM grows by dup_num, so that each recipient duplicated has a unique number allocated
        instance_num = i*dup_num

    for line_num in range(0, len(user_data)):
        if sample_entity(entity, i, total_instances_of_entity):
            user_data[line_num] = user_data[line_num].replace("IP_ADDR", ip)
        user_data[line_num] = user_data[line_num].replace("INSTANCE_NUM", f"{instance_num}")
        user_data[line_num] = user_data[line_num].replace("BKT_NAME", BKT_NAME)
        user_data[line_num] = user_data[line_num].replace("RUN_MODE", f"{run_mode}")

        if entity == "proxy" or entity == "recipient":
            user_data[line_num] = user_data[line_num].replace("BRANCHING_FACTOR", f"{proxy_tree_branching_factor}")
            user_data[line_num] = user_data[line_num].replace("REDUNDANCY_FACTOR", f"{redundancy_factor}")
            user_data[line_num] = user_data[line_num].replace("TOTAL_NON_REDUNDANT_PROXIES", f"{total_instances_of_entity}")
            user_data[line_num] = user_data[line_num].replace("DUPLICATION_FACTOR", f"{dup_num}")

        # if instance_num == 0:
        #     user_data[line_num] = user_data[line_num].replace("sysctl -w vm.nr_hugepages=4000", "# sysctl -w vm.nr_hugepages=4000")
    return user_data

def get_vm(entity, i, values, new_ips, run_mode=0, proxy_tree_branching_factor=8, total_instances_of_entity=0, total_non_redundant_proxies=0, dup_num=1, redundancy_factor=0, rcvr_mode="socket"):
    with open(f"./vm_templates/{entity}.json") as f:
        vm = json.load(f)
    ip = next_ip(values[f"base_{entity}_vm_ip"], i)
    
    vm["Properties"]["NetworkInterfaces"][0]["PrivateIpAddress"] = ip
    vm["Properties"]["NetworkInterfaces"][0]["GroupSet"][0] = values["sg_id"]
    vm["Properties"]["NetworkInterfaces"][0]["SubnetId"] = values["subnet_id"]

    if entity == "proxy":
        man_ip = next_ip(values[f"base_proxy_management_ip"], i)
        vm["Properties"]["UserData"]["Fn::Base64"]["Fn::Join"][1] = get_user_data(
            entity, i, man_ip, run_mode, proxy_tree_branching_factor, total_non_redundant_proxies, dup_num=dup_num, redundancy_factor=redundancy_factor)
    elif entity == "recipient":
        man_ip = next_ip(values[f"base_recipient_management_ip"], i)
        vm["Properties"]["UserData"]["Fn::Base64"]["Fn::Join"][1] = get_user_data(
            entity, i, man_ip, run_mode, proxy_tree_branching_factor, total_non_redundant_proxies, dup_num=dup_num, redundancy_factor=redundancy_factor, rcvr_mode=rcvr_mode)
    else:
        vm["Properties"]["UserData"]["Fn::Base64"]["Fn::Join"][1] = get_user_data(
            entity, i, ip, run_mode, proxy_tree_branching_factor, total_non_redundant_proxies, dup_num=dup_num, redundancy_factor=redundancy_factor)

    new_ips[entity] += [ip]
    return vm

def get_resource(resource_name, i, values, new_ips, for_entity=""):
    with open(f"./vm_templates/{resource_name}.json") as f:
        resource = json.load(f)

    if resource_name == "elastic_ip":
        return resource

    elif resource_name == "elastic_network_interface":
        if for_entity == "proxy":
            ip = next_ip(values[f"base_proxy_management_ip"], i)
        elif for_entity == "recipient":
            ip = next_ip(values[f"base_recipient_management_ip"], i)
        else:
            print("for_entity unknown")
            exit(1)
        resource["Properties"]["PrivateIpAddress"] = ip
        resource["Properties"]["GroupSet"][0] = values["sg_id"]
        resource["Properties"]["SubnetId"] = values["subnet_id"]
        if for_entity == "proxy":
            new_ips["proxy_management"] += [ip]
        if for_entity == "recipient":
            new_ips["recipient_management"] += [ip]
        return resource

    elif resource_name == "eip_eni_association":
        name = ""
        if for_entity == "recipient":
            name = "Recipient"
        resource["Properties"]["AllocationId"]["Fn::GetAtt"] = [ f"EIP{name}{i}", "AllocationId" ]
        resource["Properties"]["NetworkInterfaceId"]["Ref"] = f"ENI{name}{i}"
        return resource

    # Following are awstg multicast related
    elif resource_name == "mc_member":
         resource["Properties"]["NetworkInterfaceId"] = { "Fn::GetAtt": [f"ENIRecipient{i}", "Id"] }
         if i > 0:
            resource["DependsOn"] = f"MCMember{i-1}"
         return resource


### TODO: Generalize making split stack files
def make_stack_files(file_prefix, resource_types, stack):
    total_pieces = TOTAL_SPLIT_STACK_PIECES
    stack_resources = [{} for _ in range(total_pieces)]

    resources = list(stack["Resources"].keys())
    
    curr_piece = 0
    i = 0

    last_resource = ""
    iteration = 0
    while((curr_piece < total_pieces) and (i < len(resources))):
        stack_resources[curr_piece][resources[i]] = stack["Resources"][resources[i]]
        last_resource = resources[i]
        if len(stack_resources[curr_piece]) >= len(resources)/total_pieces:
            success = False

            if "RecipientVM" in resources[i] or "ProxyVM" in resources[i]:
                success = True

            if success:
                curr_piece += 1

        i += 1
        iteration += 1
        if iteration > 5000:
            break
    
    for i in range(0, total_pieces):
        with open(f"{file_prefix}{i}.json", "w") as f:
            f.write(json.dumps({"Resources": stack_resources[i]}))
            print(len(list(stack_resources[i].keys())))


def show_ip_ranges(ips):
    print("IP Ranges:")
    print("Client:              ", ips["client"][0], " -- ", ips["client"][-1])
    print("Proxy Management:    ", ips["proxy_management"][0], " -- ", ips["proxy_management"][-1])
    print("Proxy:               ", ips["proxy"][0], " -- ", ips["proxy"][-1])
    print("Recipient:           ", ips["recipient"][0], " -- ", ips["recipient"][-1])
    print("Recipient Management:", ips["recipient_management"][0], " -- ", ips["recipient_management"][-1])

def create_template(total_recipients, total_proxies=1, redundancy_factor=1, total_clients=1, run_mode=0, proxy_tree_branching_factor=8, dup_num=1, config_index=0, rcvr_mode="socket"):
    if total_recipients % dup_num:
        print("The number of recipients has to be divisible by duplication factor")
        exit(1)

    stack = {
        "Resources": {}
    }
    make_aws_config()

    with open("./template_values.json") as f:
        values = json.load(f)["configs"][config_index]

    print(json.dumps(values, indent=4))

    new_ips = {
        "client": [],
        "proxy": [],
        "proxy_management": [],
        "recipient": [],
        "recipient_management": [],
    }

    if run_mode == "awstg":
        new_ips["proxy"] = [values["multicast_ip"]]
        new_ips["proxy_management"] = [values["multicast_ip"]]

    for i in range(0, total_clients):
        stack["Resources"][f"ClientVM{i}"] = get_vm("client", i, values, new_ips, run_mode, proxy_tree_branching_factor, total_clients)

        if run_mode == "awstg":
            stack["Resources"][f"ClientVM{i}"]["Properties"]["NetworkInterfaces"] = [
                    {
                        "NetworkInterfaceId": "eni-02bfc133910fdbfb4",
                        "DeviceIndex": "0"
                    }
                ]
        stack["Resources"][f"ClientVM{i}"]["Properties"]["Tags"] = [{
                "Key": "Name",
                "Value": f"ClientVM{i}"
            }]

    total_redundant_proxies = get_total_redundant_proxies(total_proxies, proxy_tree_branching_factor, redundancy_factor)
    for i in range(0, total_proxies+total_redundant_proxies):
        stack["Resources"][f"EIP{i}"] = get_resource("elastic_ip", i, values, new_ips, "proxy")
        stack["Resources"][f"ENI{i}"] = get_resource("elastic_network_interface", i, values, new_ips, "proxy")
        stack["Resources"][f"EAssociation{i}"] = get_resource("eip_eni_association", i, values, new_ips, "proxy")
        stack["Resources"][f"EAssociation{i}"]["DependsOn"] = [f"EIP{i}", f"ENI{i}"]

        stack["Resources"][f"ProxyVM{i}"] = get_vm(
            "proxy", i, values, new_ips, run_mode, proxy_tree_branching_factor, total_proxies, total_proxies, dup_num=dup_num, redundancy_factor=redundancy_factor)
        stack["Resources"][f"ProxyVM{i}"]["DependsOn"] = f"EAssociation{i}"
        stack["Resources"][f"ProxyVM{i}"]["Properties"]["NetworkInterfaces"] += [{
            "NetworkInterfaceId" : {
                "Ref" : f"ENI{i}"
            },
            "DeviceIndex": "0"
        }]
        stack["Resources"][f"ProxyVM{i}"]["Properties"]["Tags"] = [{
                "Key": "Name",
                "Value": f"ProxyVM{i}"
            }]

    for i in range(0, int(total_recipients/dup_num)):
        if run_mode != "awstg":
            stack["Resources"][f"EIPRecipient{i}"] = get_resource("elastic_ip", i, values, new_ips, "recipient")
            stack["Resources"][f"ENIRecipient{i}"] = get_resource("elastic_network_interface", i, values, new_ips, "recipient")
            stack["Resources"][f"EAssociationRecipient{i}"] = get_resource("eip_eni_association", i, values, new_ips, "recipient")
            stack["Resources"][f"EAssociationRecipient{i}"]["DependsOn"] = [f"EIPRecipient{i}", f"ENIRecipient{i}"]

            stack["Resources"][f"RecipientVM{i}"] = get_vm("recipient", i, values, new_ips, run_mode, proxy_tree_branching_factor, total_recipients, total_proxies, dup_num, redundancy_factor=redundancy_factor, rcvr_mode=rcvr_mode)
            stack["Resources"][f"RecipientVM{i}"]["DependsOn"] = f"EAssociationRecipient{i}"
            stack["Resources"][f"RecipientVM{i}"]["Properties"]["NetworkInterfaces"] += [{
                "NetworkInterfaceId" : {
                    "Ref" : f"ENIRecipient{i}"
                },
                "DeviceIndex": "0"
            }]

        else:
            stack["Resources"][f"ENIRecipient{i}"] = get_resource("elastic_network_interface", i, values, new_ips, "recipient")
            stack["Resources"][f"RecipientVM{i}"] = get_vm("recipient", i, values, new_ips, run_mode, proxy_tree_branching_factor, total_recipients, total_proxies, dup_num, redundancy_factor=redundancy_factor)
            stack["Resources"][f"RecipientVM{i}"]["Properties"]["NetworkInterfaces"] = [{
                "NetworkInterfaceId" : {
                    "Ref" : f"ENIRecipient{i}"
                },
                "DeviceIndex": "0",
            }]
            stack["Resources"][f"MCMember{i}"] = get_resource("mc_member", i, values, new_ips, "recipient")

        stack["Resources"][f"RecipientVM{i}"]["Properties"]["Tags"] = [{
            "Key": "Name",
            "Value": f"RecipientVM{i}"
        }]
    write_new_ips_config(new_ips, values["port"], dup_num)

    with open("./_stack.json", "w") as f:
        f.write(json.dumps(stack, indent=4))
        # f.write(json.dumps(stack, indent=4))
    resource_types = ["ProxyVM", "EAssociation", "ENI", "EIP", "ClientVM", "RecipientVM"]
    make_stack_files("./_stack", resource_types, stack)
    
    #show_ip_ranges(new_ips)

def deploy_template(option=1):
    print("Creating bundle of project")
    # We fix the name for the bundle for simplicity
    comm = f"tar -czf {script_location}/bundled_proj.tar.gz --exclude=bundled_proj.tar.gz --exclude analysis --exclude analysis-me --exclude test --exclude .git -C {root_dir} ."
    print(comm)
    subprocess.run(comm, shell=True)
    print("Creating bucket named:", BKT_NAME)
    # mb can cause errors if bucket already exists but it has no effects on the deployment
    # a better way would be to check if the bucket exists and then create it, but seems redundant
    subprocess.run(f"aws s3 mb s3://{BKT_NAME} --region us-east-2", shell=True)
    print("Uploading bundle to bucket")
    upload_command = f"aws s3 cp {script_location}/bundled_proj.tar.gz s3://{BKT_NAME}/bundled_proj.tar.gz"
    print(upload_command)
    subprocess.run(upload_command, shell=True)
    if option == 1:
        subprocess.run(deploy_cmd, shell=True)
    elif option == 2:
        subprocess.run(deploy_large_cmd, shell=True)
    else:
        for i in range(0, TOTAL_SPLIT_STACK_PIECES):
            subprocess.run(f"aws cloudformation deploy --stack-name {STACK_NAME}{i} --template-file _stack{i}.json --region us-east-2 --s3-bucket cf-templates-haseeb", shell=True)

    print("Cleaning up targets")
    subprocess.run(f"rm {script_location}/bundled_proj.tar.gz", shell=True)

def main():
    arg_def = argparse.ArgumentParser()
    arg_def.add_argument(
        "action",
        choices=["deploy-stack", "delete-stack", "validate-template", "create-template"],
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
        choices=["socket", "dpdk", "iouring", "awstg"],  # while using awstg, use b = 1, p = 0
        default="socket",
        dest="run_mode",
    )
    arg_def.add_argument(
        "-rcvrm", "--rcvr-mode",  # I should just remove it. this is basically for selecting userdata file. but recipientdpdk.json can be used universally
        choices=["socket", "dpdk"],
        default="dpdk",
        dest="rcvr_mode",
    )
    arg_def.add_argument(
        "-dm", "--deploy-mode",
        choices=["standard", "large", "split"],
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
                    deploy_template()
                case "large":
                    print("Large deployment")
                    deploy_template(2)
                case "split":
                    print("Split deployment")
                    deploy_template(3)
        case "delete-stack":
            subprocess.run(delete_cmd, shell=True)
            subprocess.run(f"aws s3 rb s3://{BKT_NAME} --region us-east-2 --force", shell=True)
            print("Delete stack")
        case "validate-template":
            subprocess.run(validate_cmd, shell=True)
            print("Validate template")
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

