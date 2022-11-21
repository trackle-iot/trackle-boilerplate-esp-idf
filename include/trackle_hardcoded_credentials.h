#ifndef TRACKLE_HARDCODED_CREDENTIALS_H
#define TRACKLE_HARDCODED_CREDENTIALS_H

#include <esp_types.h>

#include <trackle_esp32.h>

// If NOT using credentials from flash memory, use hardcoded ones.
#ifndef USE_CREDENTIALS_FROM_FLASH // <-- if NOT defined

/**
 * To generate a private key-device ID pair:
 *  - Connect to your Trackle dashboard;
 *  - Click on "Claim di un dispositivo" in the top right corner;
 *  - Click on "Non hai un ID dispositivo?" in the window that opens;
 *  - Click on "Continua" in the next window;
 *  - Annotate the device ID and keep the private key file with ".der" extension that will be downloaded;
 *  - Fill the HARDCODED_DEVICE_ID array with the device ID from the previous step converted in this way: "0f4a12..." -> "0x0f, 0x4a, 0x12, ..."
 *  - Convert the previous private key file to C literal using the command: "cat private_key.der | xxd -i";
 *  - Copy the C literal version of the key (e.g. 0x5f, 0x91, ...) from the previous command output into the HARDCODED_PRIVATE_KEY array.
 *  - Remove the #error directive that prevents the firmware from building successfully.
 */

#error "Did you put the Trackle credentials in 'include/trackle_hardcoded_credentials.h' ?"

const uint8_t HARDCODED_DEVICE_ID[DEVICE_ID_LENGTH] = {
    // device ID bytes go here (e.g. 0x04, 0x58, 0xFG ...)
};

const uint8_t HARDCODED_PRIVATE_KEY[PRIVATE_KEY_LENGTH] = {
    // private key bytes go here (e.g. 0x04, 0x58, 0xFG ...)
};

#endif

#endif