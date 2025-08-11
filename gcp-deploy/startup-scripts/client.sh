#!/bin/bash -xe

echo "Starting script..."

get_metadata() {
  curl -s http://metadata.google.internal/computeMetadata/v1/instance/attributes/$1 -H "Metadata-Flavor: Google"
}

echo "Fetching metadata"

# Common Variables
IP_ADDR=$(get_metadata IP_ADDR)
INSTANCE_NUM=$(get_metadata INSTANCE_NUM)
BKT_NAME=$(get_metadata BKT_NAME)
RUN_MODE=$(get_metadata RUN_MODE)
CLOUD=$(get_metadata CLOUD)

echo "IP Address: $IP_ADDR"
echo "Instance Number: $INSTANCE_NUM"
echo "Bucket Name: $BKT_NAME"
echo "Run Mode: $RUN_MODE"
echo "Cloud: $CLOUD"

echo "Downloading service bundle"
cd /home/ubuntu
sudo rm -rf dom-tenant-service/
sudo mkdir dom-tenant-service/
sudo chmod 777 dom-tenant-service/
sudo chown -R root:root ./dom-tenant-service

gcloud storage cp gs://cdm-templates-nyu-systems-multicast/bundled_proj.tar.gz .
tar -xzf bundled_proj.tar.gz -C dom-tenant-service/
export HOME=/root

echo "Deploying ttcs"
cd dom-tenant-service/gcp-deploy
sudo chmod +x ../aws-deploy/stats_to_s3.sh
sudo chmod +x ./deploy_ttcs.sh
sudo ./deploy_ttcs.sh ./assets/ttcs-agent.cfg $IP_ADDR

cd ../src

echo "Compiling client"
make multicast_client
