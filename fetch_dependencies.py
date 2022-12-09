# -*- coding: utf-8 -*-

"""
Script to be executed from platformio.ini before build in order to fetch and untar ".tar.gz" archives from GitHub.
"""

import os
import re
import sys
import shelve
import shutil
import tarfile
from contextlib import suppress

import requests

Import("env")


DEPS_DEST_PATH = "components" # Directory where dependencies must be moved once fetched.
PLATFORMIO_INI_VAR = "custom_dependencies" # Variable inside platformio.ini that contains dependencies.
SEPARATOR = "<-" # String that separates local dependency name from URL in platformio.ini variable.
CACHE_FILE_NAME = ".fetch_dependencies.shelve" # Name of file that will contain dependencies hash.

REGEX = re.compile("https://(?:www\.)?github.com/.+/(.+)/releases/download/(.+)/.+")


def receive_file_to_disk(url, local_path):
    """ Download file from URL to specified local path on disk """
    clf_file_req = requests.get(url, stream=True)
    if clf_file_req.status_code == 200:
        with open(local_path, "wb") as file:
            for chunk in clf_file_req.iter_content(chunk_size=1024*256):
                file.write(chunk)
    else:
        raise Exception(f"Request returned {clf_file_req.status_code}")


def get_tar_root_dirs(tar):
    f1 = filter(lambda o: o.isdir(), tar.getmembers())
    f2 = filter(lambda o: "/" not in o.name, f1)
    return map(lambda o: o.name, f2)


print("Resolving dependencies...")

if not os.path.isdir(DEPS_DEST_PATH):
    os.mkdir(DEPS_DEST_PATH)

with shelve.open(CACHE_FILE_NAME) as cache:
    
    dependencies = env.GetProjectOption(PLATFORMIO_INI_VAR)
    
    if dependencies:
        
        for dep in dependencies.split():

            dep_name, url = dep.split(SEPARATOR)

            try:
                repo_name, version = re.findall(REGEX, url)[0]
            except:
                print("Make sure you are using a Github release as URL for dependency.", file=sys.stderr)
                raise
            
            # Get MD5 hashes (both base64 encoded)
            cached_md5 = cache.get(dep_name) or "$invalid$base64$"
            fetched_md5 = requests.get(url, stream=True).headers.get("content-md5") or "&invalid&base64&"
            
            if dep_name not in os.listdir(DEPS_DEST_PATH) or cached_md5 != fetched_md5:

                print(f"Dependency \"{dep_name}\" not found locally or different from required. Will be downloaded.")

                # Paths
                dir_path = os.path.join(DEPS_DEST_PATH, dep_name)
                tar_path = dir_path + ".tar.gz"

                # Downloading new dependency
                print(f"Downloading {repo_name} {version} from Github for \"{dep_name}\" ... ", end="", flush=True)
                receive_file_to_disk(url, tar_path)
                print("Done")

                # Untar downloaded dependency
                print(f"Extracting {repo_name} into {dir_path} ... ", end="", flush=True)
                with suppress(FileNotFoundError):
                    shutil.rmtree(dir_path)
                with tarfile.open(tar_path) as tar:
                    root_dir, = get_tar_root_dirs(tar) # Only 1 root directory must exist! (comma after var name is important!)
                    tar.extractall(DEPS_DEST_PATH)
                os.rename(os.path.join(DEPS_DEST_PATH, root_dir), dir_path)
                os.remove(tar_path)
                print("Done")

                # Save new file hash
                cache[dep_name] = fetched_md5

            else:
                print(f"Dependency \"{dep_name}\" already satisfied.")

print("Dependencies resolved successfully")