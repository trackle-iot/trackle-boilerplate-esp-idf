# Template project for Trackle connected firmware

<p align="center">
<br><img src="https://www.trackle.io/wp-content/uploads/2022/06/iot-platform-trackle.png" alt="drawing" width="100"/></p>

## Table of contents

- [Template project for Trackle connected firmware](#template-project-for-trackle-connected-firmware)
  - [Table of contents](#table-of-contents)
  - [Content of the repository](#content-of-the-repository)
  - [Setup of a new project](#setup-of-a-new-project)
  - [Development environment](#development-environment)
  - [Functionality provided by this template](#functionality-provided-by-this-template)
    - [Connection to the cloud](#connection-to-the-cloud)
      - [Get a Device ID and a private key](#get-a-device-id-and-a-private-key)
      - [Credential security and encryption](#credential-security-and-encryption)
      - [Provisioning of Wi-fi through Bluetooth LE](#provisioning-of-wi-fi-through-bluetooth-le)
    - [Cloud features examples](#cloud-features-examples)
    - [OTA updates](#ota-updates)
      - [OTA Security Configuration](#ota-security-configuration)
  - [Setting of product ID](#setting-of-product-id)

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
* The Device ID will be shown on the screen and the private key file will be downloaded as <device_id>.der where <device_id> is the Device ID from Trackle.

Device ID and the private key are the credentials needed by Trackle cloud to authenticate and identify the device.

In order to connect to the cloud (and even to be able to build the firmware), one must decide if connection will be performed using:
  * Hardcoded credentials;
  * Credentials from internal flash storage (`USE_CREDENTIALS_FROM_FLASH`).

In the first case, credentials must be provided to the firmware by putting them in the source code (see instructions in [trackle_hardcoded_credentials.h](include/trackle_hardcoded_credentials.h)). This solution is provided since it's the quickest and it's thought to be used for tests.

In the second case (`USE_CREDENTIALS_FROM_FLASH`), credentials are read at boot from the factory partition. By default they are treated as plain; if the flag `encrypted=1` is present, the private key is decrypted with the eFuse AES key, while the Device ID remains plain. If credentials are missing, the firmware requests them over UART (UDC) and can store the private key encrypted when `encrypted=1` is provided.

To pre-create the partition via the [NVS Partition Generator Utility](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_partition_gen.html), use the following CSV File Format:

```
key,type,encoding,value
device,namespace,,
device_id,data,hex2bin,<device_id>
private_key,file,binary,<device_id>.der
```

The choice is made through the declaration of the `USE_CREDENTIALS_FROM_FLASH` constant in `platformio.ini`.

#### Credential security and encryption

The library encrypts credentials using an AES-128 key stored in eFuse and loaded automatically by `initStorage()`:

- **Symmetric key in eFuse**: `initStorage()` calls `trackle_crypto_init()`, which looks for a key in BLK3. If none is found, it generates a random 128-bit key and burns it into the fuses. This is irreversible and binds the key to the device.
- **Device ID and private key in NVS**: when `USE_CREDENTIALS_FROM_FLASH` is active, the library reads credentials from the factory partition via `readDeviceInfoFromStorage()`. If it finds `encrypted=1`, it decrypts the stored private key with the eFuse AES key; otherwise it assumes the credentials are stored in plain. Device ID is always stored and read in plain.
- **Encrypted Wi‑Fi credentials**: `writeWifiConfigToStorage()` encrypts SSID and password with the same AES key and saves them to the `config` partition (namespace `machine`, key `wifi_creds`). `readWifiConfigFromStorage()` decrypts them at boot and `app_main()` passes them to `wifi_set_credentials()`. If missing, it logs the error and Wi‑Fi remains to be provisioned.

#### Provisioning of Wi-fi through Bluetooth LE

Provisioning of Wi-fi credentials to the device can be done using [ESP BLE Provisioning](https://play.google.com/store/apps/details?id=com.espressif.provble): Espressif's official provisioning app.

In order to connect to the device using the app, it's necessary to keep pressed the FLASH button on the device for 10 seconds. After this step, the app will be able to find the device and connect to it.

> **_NOTE:_**  The FLASH button is the one that, on most boards, is used to put the device in upload mode in order to flash a new firmware. It may be labeled with different names (e.g. BOOT), or miss a label. In case the board doesn't have this button, one can bring low the GPIO0 (GPIO zero) pin of ESP32 module for 10 seconds to enable provisioning.

### Cloud features examples

The following features are shown in the firmware:
  * POST functions;
  * Variables GET;
  * GET through functions;
  * Events publishing.

### OTA updates

OTA updates are enabled out of the box. New firmware versions can be flashed by providing a valid URL to the device through the Trackle dashboard.

The URL must point to a ".bin" compiled firmware hosted on a publicly accessible webserver.

#### OTA Security Configuration

The firmware includes support for OTA signature verification and HTTPS certificate validation. To enable these security features:

1. **Generate Public/Private Key Pair for OTA Verification**

   Trackle OTA verification uses an **ECC P-256 (secp256r1)** public/private key pair. The **private key** is used to sign the firmware, while the **public key** is embedded in your ESP32 firmware to verify the signature.

   Steps to generate keys:

   ```bash
   # 1) Generate the private key
   openssl ecparam -name prime256v1 -genkey -noout -out private.pem

   # 2) Generate the public key (PEM format)
   openssl ec -in private.pem -pubout -out public.pem

   # 3) Export the public key in DER format
   openssl ec -in private.pem -pubout -outform DER -out public.der

   # 4) Convert the DER public key into a C array
   xxd -i public.der > firmware_key.c
   ```

2. **Configure OTA Certificate and Verification Key**

   In `src/main.c`, you need to configure:
   - `digicert_global_root_g2_pem`: The DigiCert Global Root G2 certificate in PEM format for HTTPS server certificate validation. You can download it from: https://www.digicert.com/kb/digicert-root-certificates.htm
   - `firmware_public_key`: The public key array (from step 4 above) for firmware signature verification

   Example configuration:

   ```c
   // OTA certificate and verification key
   const char *digicert_global_root_g2_pem = "-----BEGIN CERTIFICATE-----\n...";
   const uint8_t firmware_public_key[] = {0x30, 0x59, ...}; // From firmware_key.c
   const size_t firmware_public_key_size = sizeof(firmware_public_key);
   ```

3. **Sign Your Firmware**

   If the device is part of a product and the product has signature verification configured in the Trackle console, the firmware will be automatically signed when uploaded through the console. The signature will be verified by the device using the public key embedded in the firmware.

> **Note:** If OTA verification is not configured, the device will still accept OTA updates, but without signature verification. For production deployments, it's strongly recommended to enable OTA verification.

## Setting of product ID

Product ID can be set by declaring the `IS_PRODUCT` and `PRODUCT_ID` constants in `platformio.ini`. While initializing `IS_PRODUCT` with a value is not mandatory, `PRODUCT_ID` on the contrary must be initialized with the desired product ID.

The project can be compiled even if the current device is not a product (as intended in the context of a Trackle connected application).
