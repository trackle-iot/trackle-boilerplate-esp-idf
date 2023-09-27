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

Import("env")

PLATFORMIO_INI_VAR = "custom_github_assets" # Variable inside platformio.ini that contains dependencies.
SEPARATOR = "<-" # String that separates local dependency name from URL in platformio.ini variable.

dependencies = env.GetProjectOption(PLATFORMIO_INI_VAR)

if dependencies:
    
    dependencies_list = ""

    for dep in dependencies.split():

        dep_name, tar, version, _ = dep.split(SEPARATOR)

        try:
            dependencies_list += dep_name + ":" + version + " "
            # print("Repo name, version", dep_name, version)
        except:
            raise
    
    print("COMPONENTS_LIST: " + dependencies_list);
    env.Append(CPPDEFINES=("COMPONENTS_LIST", "\\\"" + dependencies_list + "\\\""))