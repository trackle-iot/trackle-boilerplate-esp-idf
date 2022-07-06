#include <nvs_flash.h>
#include <esp_log.h>

#include <trackle_esp32.h>

static const char *TAG = "main";

void app_main()
{
    esp_err_t err = ESP_OK;

    err = nvs_flash_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error initializing NVS: %s", esp_err_to_name(err));
        return;
    }
}