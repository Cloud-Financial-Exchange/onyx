import sys
import subprocess
import os

bucket = "gs://melogs/"
download_dir = "assets/"

def get_upload_command(fp):
    command = f"gsutil cp {fp} {bucket}{fp}"
    return command

def get_download_command(fp):
    command = f"gsutil cp {bucket}{fp} {os.path.join(download_dir, fp)}"
    return command

def main():
    if len(sys.argv) < 3:
        print("Usage: script.py <mode> <file_paths>")
        print("Mode: upload/u or download/d")
        sys.exit(1)

    mode = sys.argv[1].lower()
    file_paths = sys.argv[2:]

    if mode not in ("upload", "u", "download", "d"):
        print("Invalid mode. Use 'upload/u' or 'download/d'.")
        sys.exit(1)

    for fp in file_paths:
        if mode in ("upload", "u"):
            command = get_upload_command(fp)
        else:
            command = get_download_command(fp)

        print(f"Running: {command}")

        # Run command
        try:
            result = subprocess.run(command, shell=True, capture_output=True, text=True, check=True)
            print("Success")
        except subprocess.CalledProcessError as e:
            print("Failed")
            print(f"Error: {e.stderr}")

    # Print all file paths in one line
    for fp in file_paths:
        if mode in ("download", "d"):
            print(f"{download_dir}{fp}", " ", end="")
        else:
            print(f"{fp}", " ", end="")
    print()  # Ensure the final output ends with a newline

if __name__ == "__main__":
    main()
