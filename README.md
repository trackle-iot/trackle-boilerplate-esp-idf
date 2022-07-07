<center><img src="https://www.trackle.io/wp-content/uploads/2022/06/iot-platform-trackle.png" alt="drawing" width="100"/></center>

# Template project for Trackle connected firmware

This repository contains a skeleton project that was created with the intent of providing a base for the development of future applications.

The project is **fully configured to connect to the cloud** (except for credentials), and comes **with a good amount of boilerplate code** already written.

In order to clone the repository with all the required submodules, the following command can be used:

```
git clone --recurse-submodules https://github.com/trackle-iot/trackle-firmware-template-project.git
```

## Development environment

The project was created with PlatformIO inside Visual Studio Code, so this template is tought to be used inside such environment.

Usage of the code in other environments is currently untested.

## Functionality

Once compiled, the firmware provides the following functionality:
  * Connection to the Trackle cloud through Wi-fi;
  * Provisioning of Wi-fi connection credentials through Bluetooth LE;
  * Usage examples of cloud related main features;
  * OTA updates enabled out of the box.

### Connection to the cloud

In order to connect to the cloud (and even to be able to build the firmware), one must decide if connection will be performed using:
  * Hardcoded credentials;
  * Credentials from internal flash storage.

In the first case, credentials must be provided to the firmware by putting them in the source code (see instructions in [trackle_hardcoded_credentials.h](include/trackle_hardcoded_credentials.h)). This solution is provided since it's the quickest and it's tought to be used for tests.

In the second case, credentials are taken from the flash storage, so they must have been previously written there using the [credentials management tools](https://github.com/trackle-iot/trackle-device-id-generator.git).

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

## Product ID

Product ID can be set by declaring the `IS_PRODUCT` and `PRODUCT_ID` constants in `platformio.ini`. While intializing `IS_PRODUCT` with a value is not mandatory, `PRODUCT_ID` on the contrary must be initialized with the desired product ID.

The project can be compiled even if the current device is not a product (as intended in the context of a Trackle connected application).