#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_common.h"
#include "esp_flash_spi_init.h"
#include "esp_flash.h"
#include "esp_partition.h"

#include "esp_littlefs.h"

#include "esp_log.h"

static const char* TAG = "flash_ext";

void app_main(void) {
    ESP_LOGW(TAG, "Initializing external spi flash");

    esp_err_t ret;
    spi_host_device_t spi_host = SPI3_HOST;

    spi_bus_config_t bus_cfg = {
        .miso_io_num = SPI3_IOMUX_PIN_NUM_MISO,
        .mosi_io_num = SPI3_IOMUX_PIN_NUM_MOSI,
        .sclk_io_num = SPI3_IOMUX_PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ret = spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    esp_flash_t* flash_chip;

    esp_flash_spi_device_config_t flash_cfg = {
        .host_id = spi_host,
        .cs_io_num = SPI3_IOMUX_PIN_NUM_CS,
        .io_mode = SPI_FLASH_DIO,
        .speed = ESP_FLASH_40MHZ,
        .input_delay_ns = 0,
        .cs_id = 0,
    };

    ret = spi_bus_add_flash_device(&flash_chip, &flash_cfg);
    ESP_ERROR_CHECK(ret);

    ret = esp_flash_init(flash_chip);
    ESP_ERROR_CHECK(ret);

    bool isInit = esp_flash_chip_driver_initialized(flash_chip);

    if (isInit) {
        ESP_LOGW(TAG, "Success");
    } else {
        ESP_LOGE(TAG, "Failed");
    }

    uint32_t id;
    esp_flash_read_id(flash_chip, &id);
    ESP_LOGI(TAG, "Initialized external Flash, size=%d KB, ID=0x%x", flash_chip->size / 1024, id);

    const char* flash_partition_label = "eventlog";
    esp_partition_t* partition;
    ret = esp_partition_register_external(
        flash_chip, 0, 0xF00000, flash_partition_label,
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, &partition
    );

    ESP_LOGI(TAG, "Registering partition on external flash %s", esp_err_to_name(ret));

    if (partition != NULL) {
        ESP_LOGI(TAG, "part_label: %s", partition->label);
        ESP_LOGI(TAG, "start_addr: 0x%x", partition->address);
        ESP_LOGI(TAG, "size: 0x%x", partition->size);
    }

    ESP_LOGI(TAG, "Initializing littlefs");
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/eventlog",
        .partition_label = flash_partition_label,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
            if (ret == ESP_FAIL) {
                    ESP_LOGE(TAG, "Failed to mount or format filesystem");
            } else if (ret == ESP_ERR_NOT_FOUND) {
                    ESP_LOGE(TAG, "Failed to find LittleFS partition");
            } else {
                    ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
            }
            return;
    } else {
        ESP_LOGE(TAG, "Init littlefs MANTAP");
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    FILE *f = fopen("/eventlog/restart_event.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
    } else {
        char line[50];
        fgets(line, sizeof(line), f);
        fclose(f);
        // strip newline
        char *pos = strchr(line, '\n');
        if (pos) {
            *pos = '\0';
        }
        ESP_LOGI(TAG, "Read from file: %s", line);
    }

    srand(esp_timer_get_time());

    uint32_t restart_delay = rand() % 10000;
    ESP_LOGI(TAG, "restart in %d", restart_delay);
    vTaskDelay(pdMS_TO_TICKS(restart_delay));

    f = fopen("/eventlog/restart_event.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    fprintf(f, "Restart event after %d ms\n", restart_delay);
    fclose(f);

    esp_restart();

    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}