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

// FreeRTOS includes
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

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
#include <trackle_utils_properties.h>
#include <trackle_utils_notifications.h>

// Local firmware includes
#include "trackle_hardcoded_credentials.h"

#define MAIN_LOOP_PERIOD_MS 10 // Main loop period in milliseconds

static const char *TAG = "main";

// Cloud POST functions
static int funSuccess(const char *args);
static int funFailure(const char *args);
static int incrementCloudNumber(const char *args);

// Cloud GET functions
static const void *getCloudNumberMessage(const char *args);
static const void *getHalfCloudNumber(const char *args);

// Cloud GET variables
static int cloudNumber = 0;

// Properties declarations
Trackle_PropID_t propRealEditable = Trackle_PropID_ERROR;
Trackle_PropID_t propRealNotEditable = Trackle_PropID_ERROR;

// Notifications declarations
Trackle_NotificationID_t notifButtonPressed = Trackle_NotificationID_ERROR;

static int updatedPropertyCustomCallback(const char *key, const char *arg, bool isOwner); // Custom callback for properties changes that come from cloud.

static void monitorFlashButtonForProvisioning(); // Periodically check if provisioning button is pressed.
static void publishIfPeriodElapsed();            // Periodically publish event

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

    // Registering properties and property groups
    propRealEditable = Trackle_Prop_create("rex", 1000, 3, 0);
    propRealNotEditable = Trackle_Prop_create("rnex", 1000, 3, 0);
    Trackle_PropGroupID_t propGroup1 = Trackle_PropGroup_create(5000, true);
    Trackle_PropGroup_addProp(propRealEditable, propGroup1);
    Trackle_PropGroup_addProp(propRealNotEditable, propGroup1);

    // Registering notifications
    notifButtonPressed = Trackle_Notification_create("button", "inputs/btn", "%s pressed: %u, value: %s", 1, 0, 0);

    // Callback for cloud editable properties
    trackleSetUpdateStateCallback(trackle_s, updatedPropertyCustomCallback);

    // Enabling OTA
    trackleSetFirmwareUrlUpdateCallback(trackle_s, firmware_ota_url);

    // Perform connection to Trackle
    connectTrackle();

    // Start properties serving
    Trackle_Props_startTask();

    // Start notifications serving
    Trackle_Notifications_startTask();

    TickType_t latestIterationTime = xTaskGetTickCount();
    for (;;)
    {
        trackle_utils_wifi_loop();
        trackle_utils_bt_provision_loop();
        monitorFlashButtonForProvisioning();
        publishIfPeriodElapsed();
        vTaskDelayUntil(&latestIterationTime, MAIN_LOOP_PERIOD_MS / portTICK_PERIOD_MS);
    }
}

// BEGIN -- Cloud POST functions --------------------------------------------------------------------------------------------------------------------

static int funSuccess(const char *args)
{
    return 1;
}

static int funFailure(const char *args)
{
    return -1;
}

static int incrementCloudNumber(const char *args)
{
    cloudNumber++;
    return 1;
}

// END -- Cloud POST functions ----------------------------------------------------------------------------------------------------------------------

// BEGIN -- Cloud GET functions --------------------------------------------------------------------------------------------------------------------

static char cloudNumberMessage[64];
static const void *getCloudNumberMessage(const char *args)
{
    sprintf(cloudNumberMessage, "The number is %d !", cloudNumber);
    return cloudNumberMessage;
}

static const void *getHalfCloudNumber(const char *args)
{
    static char buffer[40];
    buffer[0] = '\0';
    sprintf(buffer, "{\"halfCloudNumber\":%d}", cloudNumber / 2);
    return buffer;
}

// END -- Cloud GET functions ----------------------------------------------------------------------------------------------------------------------

static int updatedPropertyCustomCallback(const char *key, const char *arg, bool isOwner)
{
    if (strcmp(key, "rex") == 0)
    {
        double doubleValue = 0;
        if (sscanf(arg, "%lf", &doubleValue) < 1)
        {
            return -1;
        }
        Trackle_Prop_update(propRealEditable, doubleValue * 1000);
        ESP_LOGI(TAG, "rex property updated to: %.3f", Trackle_Prop_getValue(propRealEditable) / (double)Trackle_Prop_getScale(propRealEditable));
        return 1;
    }
    else if (strcmp(key, "rnex") == 0)
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
        Trackle_Notification_update(notifButtonPressed, 1, 0);
    }
    else
    {
        pressedMillis = 0;
        Trackle_Notification_update(notifButtonPressed, 0, 0);
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