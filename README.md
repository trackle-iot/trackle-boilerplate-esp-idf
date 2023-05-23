# Template project for Trackle connected firmware

<p align="center">
<br><img src="https://www.trackle.io/wp-content/uploads/2022/06/iot-platform-trackle.png" alt="drawing" width="100"/></p>

## Table of contents

1. [Content of the repository](#content-of-the-repository)
2. [Setup of a new project](#setup-of-a-new-project)
3. [Development environment](#development-environment)
4. [Functionality provided by this template](#functionality-provided-by-this-template)
5. [Setting of product ID](#setting-of-product-id)

## Content of the repository

This repository contains a skeleton project that was created with the intent of providing a base for the development of future applications that want to connect to the Trackle platform through an ESP32 device.

The project is **fully configured to connect to the cloud** (except for credentials), and comes **with a good amount of boilerplate code** already written.

## Setup of a new project

To create a new project based on this template, please follow these steps:

1. Clone the repository:

```
git clone https://github.com/trackle-iot/trackle-boilerplate-esp-idf.git <new_folder_name>
```

2. Move to the project directory:

```
cd <new_folder_name>
```

3. Remove the original repository from remotes. By doing this, you avoid that, by doing `git push`, these changes end up in the template project's repository:

```
git remote remove origin
```

4. Create a new empty repository (no default README or `.gitignore`) on Github, where the new project will be pushed;

5. Add the new repository as origin of the new project and push changes to it:

```
git remote add origin <new_repository_url>
git branch -M main
git push -u origin main
```

From now on, all the commits that will be pushed with the `git push` command will be pushed to the new repository.

## Development environment

The project was created with PlatformIO inside Visual Studio Code, so this template is thought to be used inside such environment.

The set-up environment takes care of downloading the necessary libraries specified using the `custom_github_assets` variable defined in `platformio.ini`. The libraries specified therein must be existing TAR archives available as Github assets inside a release. For a description of the format used inside this variable, please see the related comment inside `platformio.ini`.

In order for the described mechanism to work, it's necessary that the `fetch_github_assets.py` script is present in the root directory of the project. 

If access to private repositories is required, the previous script needs that the file `.pio_github_token` is present in user's home directory and that such file contains a valid Github access token. This token must have permissions to access the repositories containing assets specified in `custom_github_assets` configuration variable. If the token file doesn't exist, or if its empty, only public repositories can be accessed.

Please note that, in order to use the `fetch_github_assets.py` script, the [PyGithub](https://pypi.org/project/PyGithub/) library must be installed in Platformio's Python interpreter.

The following shell session shows how to install this library:
```
cd ~/.platformio/penv/bin
source activate
pip3 install PyGithub
```

## Functionality provided by this template

Once compiled, the firmware provides the following functionality:
  * Connection to the Trackle cloud through Wi-fi;
  * Provisioning of Wi-fi connection credentials through Bluetooth LE;
  * Usage examples of cloud related main features;
  * OTA updates enabled out of the box.

### Connection to the cloud

#### Get a Device ID and a private key
* Create an account on Trackle Cloud (https://trackle.cloud/)
* Open "My Devices" section from the drawer
* Click the button "Claim a device"
* Select the link "I don't have a device id", then Continue
* The Device ID will be shown on the screen and the private key file will be download with name <device_id>.der where <device_id> is Device ID taken from Trackle.

Device ID and the private key are the credentials needed by Trackle cloud to authenticate and identify the device.

In order to connect to the cloud (and even to be able to build the firmware), one must decide if connection will be performed using:
  * Hardcoded credentials;
  * Credentials from internal flash storage.

In the first case, credentials must be provided to the firmware by putting them in the source code (see instructions in [trackle_hardcoded_credentials.h](include/trackle_hardcoded_credentials.h)). This solution is provided since it's the quickest and it's thought to be used for tests.

In the second case, credentials are taken from the flash storage, so they must have been previously created and written using [NVS Partition Generator Utility](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_partition_gen.html) with the following CSV File Format:

```
key,type,encoding,value
device,namespace,,
device_id,data,hex2bin,<device_id>
private_key,file,binary,<device_id>.der
```

The choice is made through the declaration of the `USE_CREDENTIALS_FROM_FLASH` constant in `platformio.ini`.

### Provisioning of Wi-fi through Bluetooth LE

Provisioning of Wi-fi credentials to the device can be done using [ESP BLE Provisioning](https://play.google.com/store/apps/details?id=com.espressif.provble): Espressif's official provisioning app.

In order to connect to the device using the app, it's necessary to keep pressed the FLASH button on the device for 10 seconds. After this step, the app will be able to find the device and connect to it.

> **_NOTE:_**  The FLASH button is the one that, on most boards, is used to put the device in upload mode in order to flash a new firmware. It may be labeled with different names (e.g. BOOT), or miss a label. In case the board doesn't have this button, one can bring low the GPIO0 (GPIO zero) pin of ESP32 module for 10 seconds to enable provisioning.

### Cloud features examples

The following features are shown in the firmware:
  * POST functions;
  * Variables GET;
  * GET through functions;
  * Properties;
  * Notifications;
  * Events publishing.

### OTA updates

OTA updates are enabled. New firmware versions can be flashed by providing a valid URL to the device through the Trackle dashboard.

The URL must point to a ".bin" compiled firmware hosted on a publicly accessible webserver.

## Setting of product ID

Product ID can be set by declaring the `IS_PRODUCT` and `PRODUCT_ID` constants in `platformio.ini`. While intializing `IS_PRODUCT` with a value is not mandatory, `PRODUCT_ID` on the contrary must be initialized with the desired product ID.

The project can be compiled even if the current device is not a product (as intended in the context of a Trackle connected application).
