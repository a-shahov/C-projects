#include "custom_setup.h"

static const char *const TAG = "Setup";

int init_nvs()
{
    /* 1 - success, 0 - failed */

    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "nvs_flash_erase is failed!");
            goto failed;
        }
        ret = nvs_flash_init();
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "nvs_flash_init is failed!");
            goto failed;
        }
    }

    goto end;
failed:
    return 0;
end:
    return 1;
}