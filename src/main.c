/**
 ******************************************************************************
  Copyright (c) 2022 IOTREADY S.r.l.

  This software is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

// Standard library includes
#include <string.h>
#include <time.h>
#include <stdio.h>

// FreeRTOS includes
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

// Other ESP-IDF includes
#include <nvs_flash.h>
#include <esp_log.h>
#include <driver/gpio.h>

// Trackle libraries includes
#include <trackle_esp32.h>
#include <trackle_utils_storage.h>
#include <trackle_utils_bt_provision.h>
#include <trackle_utils_wifi.h>
#include <trackle_utils_ota.h>

// Local firmware includes
#include "trackle_hardcoded_credentials.h"

#define MAIN_LOOP_PERIOD_MS 10 // Main loop period in milliseconds

static const char *TAG = "main";

// Cloud POST functions
static int funSuccess(const char *paramString, bool isOwner, const char* funKey);
static int funFailure(const char *paramString, bool isOwner, const char* funKey);
static int incrementCloudNumber(const char *paramString, bool isOwner, const char* funKey);

// Cloud GET functions
static void *getCloudNumberMessage(const char *paramString, const char* varKey);
static void *getHalfCloudNumber(const char *paramString, const char* varKey);
static void *getLargeVariable(const char *paramString, const char* varKey);

// Cloud GET variables
static int cloudNumber = 0;

// Local variables for property updates
static double rex = 0.0;
static double rnex = 0.0;

// OTA certificate and verification key
// TODO: Replace with actual DigiCert Global Root G2 certificate in PEM format
// Download from: https://www.digicert.com/kb/digicert-root-certificates.htm
const char *digicert_global_root_g2_pem = NULL; // Set to your certificate string

// TODO: Replace with actual firmware public key for OTA signature verification
// This should be the public key corresponding to the private key used to sign the firmware
// Format: DER binary (typically 65 bytes for secp256r1 uncompressed public key)
const uint8_t firmware_public_key[] = {0x00}; // Placeholder - replace with actual key
const size_t firmware_public_key_size = sizeof(firmware_public_key);

static int updatedPropertyCustomCallback(const char *function_key, const char *arg, ...); // Custom callback for properties changes that come from cloud.

static void monitorFlashButtonForProvisioning(); // Periodically check if provisioning button is pressed.
static void publishIfPeriodElapsed();            // Periodically publish event
static void logTaskStatus();                     // Periodically log task status

void app_main()
{
    esp_err_t err = ESP_OK;

    // Initialize NVS
    err = nvs_flash_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error initializing NVS: %s", esp_err_to_name(err));
        return;
    }

    // Wifi and BT provisioning setup
    wifi_init();
    wifi_init_sta();

    trackle_utils_bt_provision_set_device_name("TrackleBpPrj");
    trackle_utils_bt_provision_init();

    // Fetching Trackle credentials (from flash storage or from hardcoded values)
#ifdef USE_CREDENTIALS_FROM_FLASH
    ESP_LOGI(TAG, "Using credentials from flash storage.");
    initStorage(true);
    err = readDeviceInfoFromStorage();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading Trackle credentials from flash: err=%s", esp_err_to_name(err));
        return;
    }
#else
    ESP_LOGI(TAG, "Using hardcoded credentials.");
    memcpy(device_id, HARDCODED_DEVICE_ID, sizeof(uint8_t) * DEVICE_ID_LENGTH);
    memcpy(private_key, HARDCODED_PRIVATE_KEY, sizeof(uint8_t) * PRIVATE_KEY_LENGTH);
#endif

    // Print device information
    // ESP_LOGE(TAG, "Firmware version: %d", FIRMWARE_VERSION);
    ESP_LOGE(TAG, "Device ID (on next line):");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, device_id, 12, ESP_LOG_ERROR);
    ESP_LOGE(TAG, "Log level: %d", LOG_LOCAL_LEVEL);
#ifdef PRODUCT_ID
    ESP_LOGE(TAG, "Product ID: %d", PRODUCT_ID);
#endif

    // Init Trackle
    initTrackle();
    Trackle_loadClaimCode();

    // Set cloud credentials
    trackleSetKeys(trackle_s, private_key);
    trackleSetDeviceId(trackle_s, device_id);
    // trackleSetFirmwareVersion(trackle_s, FIRMWARE_VERSION);

    // Set GPIO0 (FLASH/BOOT button on most boards) as input, to start provisioning through bluetooth
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);

    // Registering POST functions callable from cloud
    tracklePost(trackle_s, "funSuccess", funSuccess, ALL_USERS);
    tracklePost(trackle_s, "funFailure", funFailure, ALL_USERS);
    tracklePost(trackle_s, "incrementCloudNumber", incrementCloudNumber, ALL_USERS);

    // Registering values GETtable from cloud as result of a function call
    trackleGet(trackle_s, "getCloudNumberMessage", getCloudNumberMessage, VAR_STRING);
    trackleGet(trackle_s, "getHalfCloudNumber", getHalfCloudNumber, VAR_JSON);
    trackleGet(trackle_s, "getLargeVariable", getLargeVariable, VAR_JSON);

    // Callback for cloud editable properties
    trackleSetUpdateStateCallback(trackle_s, updatedPropertyCustomCallback);

    // Enabling OTA
    trackleSetOtaUpdateCallback(trackle_s, firmware_ota_url);

    // callback ota from utils
    set_https_ota_certificate(digicert_global_root_g2_pem); // ok

    bool res = trackleSetOtaVerificationKey(trackle_s, firmware_public_key, firmware_public_key_size);
    if (!res)
    {
        ESP_LOGE(TAG, "Failed to set OTA verification key");
    }

    // Perform connection to Trackle
    // connectTrackle();
    xTaskCreate(&trackle_task, "trackle_task", 16384, NULL, 5, NULL);

    TickType_t latestIterationTime = xTaskGetTickCount();
    for (;;)
    {
        trackle_utils_wifi_loop();
        trackle_utils_bt_provision_loop();
        monitorFlashButtonForProvisioning();
        publishIfPeriodElapsed();
        logTaskStatus();
        vTaskDelayUntil(&latestIterationTime, MAIN_LOOP_PERIOD_MS / portTICK_PERIOD_MS);
    }
}

// BEGIN -- Cloud POST functions --------------------------------------------------------------------------------------------------------------------

static int funSuccess(const char *paramString, bool isOwner, const char* funKey)
{
    return 1;
}

static int funFailure(const char *paramString, bool isOwner, const char* funKey)
{
    return -1;
}

static int incrementCloudNumber(const char *paramString, bool isOwner, const char* funKey)
{
    int increment = atoi(paramString);
    if (increment == 0)
        return -2;
    cloudNumber += increment;
    return 1;
}

// END -- Cloud POST functions ----------------------------------------------------------------------------------------------------------------------

// BEGIN -- Cloud GET functions --------------------------------------------------------------------------------------------------------------------

static char cloudNumberMessage[64];
static void *getCloudNumberMessage(const char *paramString, const char* varKey)
{
    sprintf(cloudNumberMessage, "The number is %d !", cloudNumber);
    return cloudNumberMessage;
}

static void *getHalfCloudNumber(const char *paramString, const char* varKey)
{
    static char buffer[40];
    buffer[0] = '\0';
    sprintf(buffer, "{\"halfCloudNumber\":%d}", cloudNumber / 2);
    return buffer;
}

// Esempio di callback per valori molto lunghi (usa block2 automaticamente)
static void *getLargeVariable(const char *paramString, const char* varKey)
{
    // Questo esempio restituisce un JSON molto grande
    // La libreria userà automaticamente block2 per inviarlo in blocchi
    static char large_json[10000];
    large_json[0] = '{';
    int pos = 1;
    // Genera un JSON con molti dati
    // for (int i = 0; i < 10; i++)
    for (int i = 0; i < 40; i++)
    {
        if (i > 0)
            large_json[pos++] = ',';
        pos += snprintf(large_json + pos, sizeof(large_json) - pos,
                        "\"sensor_%d\":{\"value\":%d,\"timestamp\":%lu}",
                        i, i * 10, (unsigned long)time(NULL));
    }
    large_json[pos++] = '}';
    large_json[pos] = '\0';
    return large_json;
}

// END -- Cloud GET functions ----------------------------------------------------------------------------------------------------------------------

static int updatedPropertyCustomCallback(const char *function_key, const char *arg, ...)
{
    if (strcmp(function_key, "rex") == 0)
    {
        double doubleValue = 0;
        if (sscanf(arg, "%lf", &doubleValue) < 1)
        {
            return -1;
        }
        rex = doubleValue;
        ESP_LOGI(TAG, "rex property updated to: %.3f", rex);
        return 1;
    }
    else if (strcmp(function_key, "rnex") == 0)
    {
        return -2;
    }
    return -3; // -3 means "property not found", -2 means "property not writable", -1 means "error in updating property", 1 means "update successful"
}

static void monitorFlashButtonForProvisioning()
{
    static int pressedMillis = 0;
    // If GPIO0 button (FLASH/BOOT) is pressed, count pressure milliseconds ...
    if (!gpio_get_level(GPIO_NUM_0))
    {
        pressedMillis += MAIN_LOOP_PERIOD_MS;
    }
    else
    {
        pressedMillis = 0;
    }
    // ... and if pressure lasts more than 10s, enable bluetooth LE provisioning.
    if (pressedMillis > 10000)
    {
        xEventGroupSetBits(s_wifi_event_group, START_PROVISIONING);
        pressedMillis = 0;
        ESP_LOGI(TAG, "Starting WiFi provisioning through Bluetooth ...");
    }
}

static void publishIfPeriodElapsed()
{
    static int waitingMillis = 0;
    waitingMillis += MAIN_LOOP_PERIOD_MS;
    if (waitingMillis > 20000)
    {
        tracklePublishSecure("timed_events/every20s", "20 seconds passed");
        waitingMillis = 0;
    }
}

static char stats_buffer[4096];
static void logTaskStatus()
{
    static int waitingMillis = 0;
    waitingMillis += MAIN_LOOP_PERIOD_MS;
    if (waitingMillis > 10000)
    {
        vTaskList(stats_buffer);
        ESP_LOGI(TAG, "Task Name\tStatus\tPrio\tHWM\tTask\tAffinity");
        ESP_LOGI(TAG, "%s", stats_buffer);
        waitingMillis = 0;
        ESP_LOGI(TAG, "Free RAM: %ld bytes", esp_get_free_heap_size());
    }
}