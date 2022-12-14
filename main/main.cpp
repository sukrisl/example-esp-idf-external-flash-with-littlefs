#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "EspDataStorage.h"

static const char* TAG = "flash_ext";

EspDataStorage storage;

extern "C" void app_main(void) {
    storage.addDevice(1, STORAGE_DEVICE_TYPE_FLASH);

    storage.createPartition(1, "eventlog", "/eventlog", 0xF00000);

    storage.print("/eventlog/restart_event.txt");

    srand(esp_timer_get_time());

    uint32_t restart_delay = rand() % 30000;
    ESP_LOGI(TAG, "restart in %d", restart_delay);
    vTaskDelay(pdMS_TO_TICKS(restart_delay));

    char dataRestart[50];
    sprintf(dataRestart, "Restart event after %d ms", restart_delay);
    storage.append("/eventlog/restart_event.txt", dataRestart);

    esp_restart();

    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}