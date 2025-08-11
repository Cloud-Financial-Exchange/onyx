#!/bin/bash

# Upload combined files to S3 folder
function upload_to_s3() {
    bucket=$1
    folder=$2
    filename="$3.csv"
    cloud="$4"
    id="$5"
    stat_dir="../stats"
    pwd
    # Combine all files in one and remove them
    for file in `ls $stat_dir/*.csv`; do
        cat $file >> ./$filename
        rm $file
        echo "Combined $file"
    done

    num_lines=$(wc -l < ./$filename)
    tar -czvf ${filename}.tar.gz ./$filename
    rm ./$filename

    if [ "${cloud}" == "gcp" ]; then
        gcloud storage cp ${filename}.tar.gz gs://$bucket/$folder/${filename}_${num_lines}.tar.gz
        if [ "${id}" == "0" ]; then
            gcloud storage cp ../src/config.h gs://$bucket/$folder/config.h
        fi
    else
        aws s3 cp ${filename}.tar.gz s3://$bucket/$folder/${filename}_${num_lines}.tar.gz
        if [ "${id}" == "0" ]; then
            aws s3 cp ../src/config.h s3://$bucket/$folder/config.h
        fi
    fi

    rm ./${filename}.tar.gz
}


function help() {
  echo "Usage: stats_to_s3.sh --upload <bucket> <folder> <filename>"
  exit 1
}

echo "Current directory: $(pwd)"
while [[ $# -gt 0 ]]; do
  case $1 in
    --upload)
      shift
      if [ $# -ne 5 ]; then
          echo "S3 bucket, folder, filename, cloud, and ID are not correctly provided."
          help
          exit 1
      else
          echo "Uploading to S3"
          upload_to_s3 $1 $2 $3 $4 $5
          exit 0
        fi
      ;;
    *)
      help
      ;;
  esac
done
