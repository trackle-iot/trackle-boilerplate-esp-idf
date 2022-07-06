#include <string.h>

#include <nvs_flash.h>
#include <esp_log.h>

#include <trackle_esp32.h>
#include <trackle_utils_storage.h>
#include <trackle_utils_ota.h>

static const char *TAG = "main";

// If enabled in platformio.ini, use hardcoded credentials
#ifdef USE_HARDCODED_CREDENTIALS

const uint8_t HARDCODED_PRIVATE_KEY[] = {
#error "Private key not set" // Replace this line with private key, e.g. 0xF6, 0x07, etc. (you can use "xxd -i" for conversion from raw form)
};

const uint8_t HARDCODED_DEVICE_ID[] = {
#error "Device ID not set" // Replace this line with device ID, e.g. "10af..." -> "0x10, 0xaf, ..." etc. (you can get it from Trackle dashboard)
};

#endif

// Cloud POST functions
static int funSuccess(const char *args);
static int funFailure(const char *args);

static int updatedPropertyCustomCallback(const char *key, const char *arg, bool isOwner);

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

    // Fetching Trackle credentials (from NVS or from hardcoded values)
#ifdef USE_HARDCODED_CREDENTIALS
    ESP_LOGI(TAG, "Hardcoded credentials used for Trackle connection.");
    memcpy(device_id, HARDCODED_DEVICE_ID, sizeof(uint8_t) * 12);
    memcpy(private_key, HARDCODED_PRIVATE_KEY, sizeof(uint8_t) * 122);
#else
    ESP_LOGI(TAG, "Credentials taken from NVS used for Trackle connection.");
    initStorage(true);
    err = readDeviceInfoFromStorage();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading Trackle credentials from NVS: err=%s", esp_err_to_name(err));
        return;
    }
#endif

    // Print device information
    ESP_LOGE(TAG, "Firmware version: %d", FIRMWARE_VERSION);
    ESP_LOGE(TAG, "Device ID:");
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, device_id, 12, ESP_LOG_ERROR);

    // Set cloud credentials
    trackleSetKeys(trackle_s, NULL, private_key);
    trackleSetDeviceId(trackle_s, device_id);
    trackleSetFirmwareVersion(trackle_s, FIRMWARE_VERSION);

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
        vTaskDelay(10 / portTICK_PERIOD_MS);
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