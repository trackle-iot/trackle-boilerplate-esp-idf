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

// Local firmware includes
#include <trackle_hardcoded_credentials.h>

#define MAIN_LOOP_PERIOD_MS 10 // Main loop period in milliseconds

static const char *TAG = "main";

// Cloud POST functions
static int funSuccess(const char *args);
static int funFailure(const char *args);

static int updatedPropertyCustomCallback(const char *key, const char *arg, bool isOwner); // Custom callback for properties changes that come from cloud.

static void monitorFlashButtonForProvisioning(); // Periodically check if provisioning button is pressed.

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
    trackle_utils_bt_provision_init();

    // Fetching Trackle credentials (from NVS or from hardcoded values)
#ifdef USE_CREDENTIALS_FROM_NVS
    ESP_LOGI(TAG, "Credentials taken from NVS used for Trackle connection.");
    initStorage(true);
    err = readDeviceInfoFromStorage();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading Trackle credentials from NVS: err=%s", esp_err_to_name(err));
        return;
    }
#else
    ESP_LOGI(TAG, "Hardcoded credentials used for Trackle connection.");
    memcpy(device_id, HARDCODED_DEVICE_ID, sizeof(uint8_t) * 12);
    memcpy(private_key, HARDCODED_PRIVATE_KEY, sizeof(uint8_t) * 122);
#endif

    // Print device information
    ESP_LOGE(TAG, "Firmware version: %d", FIRMWARE_VERSION);
    ESP_LOGE(TAG, "Device ID:");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, device_id, 12, ESP_LOG_ERROR);

    // Set cloud credentials
    trackleSetKeys(trackle_s, NULL, private_key);
    trackleSetDeviceId(trackle_s, device_id);
    trackleSetFirmwareVersion(trackle_s, FIRMWARE_VERSION);

    // Set GPIO0 (FLASH/BOOT button on most boards) as input, to start provisioning through bluetooth
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);

    // Init Trackle
    initTrackle();

    // Registering POST functions callable from cloud
    tracklePost(trackle_s, "funSuccess", funSuccess, ALL_USERS);
    tracklePost(trackle_s, "funFailure", funFailure, ALL_USERS);

    // Callback for cloud editable properties
    trackleSetUpdatePropertyCallback(trackle_s, updatedPropertyCustomCallback);

    // Enabling OTA
    trackleSetFirmwareUrlUpdateCallback(trackle_s, firmware_ota_url);

    // Perform connection to Trackle
    connectTrackle();

    for (;;)
    {
        trackle_utils_wifi_loop();
        trackle_utils_bt_provision_loop();
        monitorFlashButtonForProvisioning();
        vTaskDelay(MAIN_LOOP_PERIOD_MS / portTICK_PERIOD_MS);
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

// END -- Cloud POST functions ----------------------------------------------------------------------------------------------------------------------

static int updatedPropertyCustomCallback(const char *key, const char *arg, bool isOwner)
{
    return -1; // -1 means "property not found", -2 means "error in updating property", 1 means "update successful"
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