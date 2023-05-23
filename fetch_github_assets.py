#!/usr/bin/env python3

" Download private or public Github asset "

import os
import sys
import shelve
import shutil
import tarfile
import pathlib
from functools import partial
from contextlib import suppress

import requests
import github

from platformio.project.exception import InvalidProjectConfError

Import("env")

COMPONENTS_DIR = "components" # Directory where dependencies must be moved once fetched.
PLATFORMIO_INI_VAR = "custom_github_assets" # Variable inside platformio.ini that contains dependencies.
TOKEN_FILENAME = ".pio_github_token"
SHELF_FILENAME = ".fetch_github_assets.shelve"

class InvalidPathToRelease(Exception):
    " Raise when repository or release don't exist. "
    def __init__(self, release, repository):
        Exception.__init__(self,f"Invalid path to release {release}"
                           f" in repo {repository}, or invalid access token.")

class AssetNotFound(Exception):
    " Raised when desired asset is not found "
    def __init__(self, asset_name, release, repository):
        Exception.__init__(self, f"Asset {asset_name} not found in release"
                           f"{release} of {repository}.")

class DownloadFailed(Exception):
    " Raised when download failed "
    def __init__(self, local_path, status_code):
        Exception.__init__(self, f"Download of asset {local_path} failed with"
                           f" status code {status_code}.")

def fetch_asset_url(gh_token, repo_name, release_tag, desired_asset_name):
    " Fetch download URL for desired Github release asset "
    try:
        if gh_token is None:
            release = github.Github().get_repo(repo_name).get_release(release_tag)
        else:
            release = github.Github(gh_token).get_repo(repo_name).get_release(release_tag)
    except github.GithubException as github_exception:
        raise InvalidPathToRelease(release_tag, repo_name) from github_exception
    for asset in release.assets:
        if asset.name == desired_asset_name:
            return asset.url
    raise AssetNotFound(desired_asset_name, release_tag, repo_name)

def download_asset(gh_token, asset_url, local_path, local_md5):
    " Download asset file "
    http_heads = {"Accept": "application/octet-stream"}
    if gh_token is not None:
        http_heads["Authorization"] = f"Bearer {gh_token}"
    req = requests.get(asset_url, headers=http_heads, stream=True, timeout=10)
    if req.status_code == 200:
        remote_md5 = req.headers.get("Content-MD5")
        if not local_md5 or local_md5 != remote_md5:
            with open(local_path, "wb") as downloaded_file:
                for chunk in req.iter_content(chunk_size=1024*256):
                    downloaded_file.write(chunk)
            return True, remote_md5
        return False, None
    else:
        raise DownloadFailed(local_path, req.status_code)

def get_tar_root_dirs(tar):
    " Get name of the root folder inside a TAR archive "
    tar_folders = filter(lambda o: o.isdir(), tar.getmembers())
    root_folders = filter(lambda o: "/" not in o.name, tar_folders)
    return map(lambda o: o.name, root_folders)

def untar_asset_to_component(archive_path, component_path):
    " Untar archive containing asset to component folder "
    with suppress(FileNotFoundError):
        shutil.rmtree(component_path)
    with tarfile.open(archive_path) as tar:
        root_dir, = get_tar_root_dirs(tar)
        tar.extractall(COMPONENTS_DIR)
    os.rename(os.path.join(COMPONENTS_DIR, root_dir), component_path)
    os.remove(archive_path)

# -------------------------- Fetch dependencies ---------------------------

print("Fetching components as custom Github assets...")

if not hasattr(env, "IsCleanTarget") or not env.IsCleanTarget():
    try:
        requests.head("https://www.google.it/")
    except:
        print("No network connection found. Skipping (hope components folder is already up-to-date)", file=sys.stderr)
    else:
        token_path = os.path.join(pathlib.Path.home(), TOKEN_FILENAME)
        if not os.path.exists(token_path) or (os.path.isfile(token_path) and os.stat(token_path).st_size == 0):
            print("No token found. Public repos only.")
            token = None
        else:
            try:
                with open(token_path, encoding="utf-8") as file:
                    token = file.read().strip()
            except OSError as exc:
                print(f"The file {TOKEN_FILENAME} in user's home directory must contain a valid Github token or be empty.", file=sys.stderr)
                print("Please, ensure that such file has read permissions on it.", file=sys.stderr)
                sys.exit(exc)

        if not os.path.exists(COMPONENTS_DIR) or not os.path.isdir(COMPONENTS_DIR):
            os.mkdir(COMPONENTS_DIR)

        try:
            assets_string = env.GetProjectOption(PLATFORMIO_INI_VAR)
        except InvalidProjectConfError as exc:
            if "custom_github_assets" in str(exc):
                print("No Github asset specified for download. Skipping.")
            else:
                raise exc
        else:
            if assets_string:
                assets_list = map(partial(str.split, sep="<-"), assets_string.split())
                for component, filename, tag, repo in assets_list:

                    try:
                        dwnld_url = fetch_asset_url(token, repo, tag, filename)
                    except InvalidPathToRelease as invalid_path_to_release:
                        sys.exit(invalid_path_to_release)
                    except AssetNotFound as asset_not_found:
                        sys.exit(asset_not_found)

                    with shelve.open(SHELF_FILENAME) as metadata:
                        try:
                            tar_path = os.path.join(COMPONENTS_DIR, filename)
                            dir_path = os.path.join(COMPONENTS_DIR, component)
                            curr_md5 = os.path.exists(dir_path) and os.path.isdir(dir_path) \
                                        and metadata.get(component)
                            dwnld_done, dwnld_md5 = download_asset(token, dwnld_url, tar_path, curr_md5)
                            if dwnld_done:
                                print(f" - {component} downloaded because different from local version.")
                                print("   \tUntarring... ", end="")
                                sys.stdout.flush()
                                try:
                                    untar_asset_to_component(tar_path, dir_path)
                                except Exception as exc:
                                    print("error")
                                    raise exc
                                else:
                                    print("done")
                                    metadata[component] = dwnld_md5
                                finally:
                                    sys.stdout.flush()
                            else:
                                print(f" - {component} is already up-to-date.")
                        except DownloadFailed as download_failed:
                            sys.exit(download_failed)
            else:
                print("Empty list of Github assets to download. Skipping.")
else:
    print("Cleaning target. Skipping.")