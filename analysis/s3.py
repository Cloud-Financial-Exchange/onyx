from typing import List
import boto3
import os

s3 = boto3.client('s3')
bucket_name = 'expresults'
# bucket_name = 'stats-haseeb'

def sanitize_object_name(obj_name):
    return obj_name.replace('/', '_')

'''
Downloads objects from s3 that has the provided prefix. If count is
set to -1, all the prefix matching objects are downloading otherwise
only |count| objects are downloaded. 
'''
def download_s3_data_and_get_downloaded_filenames(prefix='5000-', count=-1, local_directory='./assets/', already_downloaded=[]) -> List[str]:
    res = []
    objects = s3.list_objects(Bucket=bucket_name, Prefix=prefix)
    print("List object")
    for obj in objects.get('Contents', []):
        if count > -1 and len(res) >= count:
            break

        local_name = sanitize_object_name(obj['Key'])
        local_file_path = os.path.join(local_directory, local_name)

        if local_file_path in already_downloaded:
            continue
        
        s3.download_file(bucket_name, obj['Key'], local_file_path)
        res += [local_file_path]
        print(f'Downloaded: {local_name} to {local_file_path}')
    if (len(res) < 1):
        print("No data downloaded!")
        exit(1)
    return res