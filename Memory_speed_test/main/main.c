#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

#include <sys/time.h>

#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "FileSystem";

//CPU frequency = 240 MHz (Component config -> ESP System Settings)
//Flash SPI speed = 80 MHz, Flash size = 4 MB (Serial flasher config)
//PSRAM clock speed = 80 MHz (Component config -> ESP PSRAM -> Support -> SPI RAM config -> Set RAM clock speed)

#define TEST_CASE_FLASH
//#define TEST_CASE_PSRAM
//#define TEST_CASE_SD

//ESP32 uses a two-way set-associative cache. Each of the two CPUs has 32 KB of cache featuring a block
//size of 32 bytes for accessing external storage

#define NUM_OF_ARRAY_CASES 3 
uint16_t arraySize[NUM_OF_ARRAY_CASES] = {10, 100, 1000};

void app_main(void)
{
    vTaskDelay(3000 / portTICK_PERIOD_MS); //wait for opening the monitor

    printf("Running program on esp32...\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    //Structure for time measuring
    struct timeval tv_now;
    char readPath[30] = "/spiffs/read.bin";
    char writePath[30] = "/spiffs/write.bin";

    #ifdef TEST_CASE_FLASH
    printf("\n******** Test case FLASH ********\n");

    //Init file system
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Check consistency of reported partition size info.
    if (used > total) {
        ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return;
        } else {
            ESP_LOGI(TAG, "SPIFFS_check() successful");
        }
    }

    printf("* Reading:\n");
    
    for (uint8_t i = 0; i < NUM_OF_ARRAY_CASES; i++) {
        uint8_t *testArray = (uint8_t *)malloc(arraySize[i]); 

        FILE* f = fopen(readPath, "rb");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return;
        }

        //Start measuring time
        gettimeofday(&tv_now, NULL);
        int64_t time_us_start = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;	
        
        for (uint16_t iter = 0; iter < 100; iter++)
            fread(testArray, 1, arraySize[i], f);

        //Finish measuring time
        gettimeofday(&tv_now, NULL);
        int64_t time_us_finish = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
        printf("[testArray size %d] Time spent for reading: ", arraySize[i]);
        printf("%lld", time_us_finish - time_us_start);
        printf(" us.\n");      

        fclose(f);  
        free(testArray);
    }

    printf("\n");
    printf("* Writing:\n");
    for (uint8_t i = 0; i < NUM_OF_ARRAY_CASES; i++) {
        uint8_t *testArray = (uint8_t *)malloc(arraySize[i]); 
        for(uint16_t j = 0; j < arraySize[i]; j++)
            testArray[j] = 0xff;

        FILE* f = fopen(writePath, "wb");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return;
        }

        //Start measuring time
        gettimeofday(&tv_now, NULL);
        int64_t time_us_start = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;	
        
        for (uint16_t iter = 0; iter < 100; iter++)
            fwrite(testArray, 1, arraySize[i], f);

        //Finish measuring time
        gettimeofday(&tv_now, NULL);
        int64_t time_us_finish = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
        printf("[testArray size %d] Time spent for writing: ", arraySize[i]);
        printf("%lld", time_us_finish - time_us_start);
        printf(" us.\n");      

        fclose(f);  
        free(testArray);
    }

    #endif

    #ifdef TEST_CASE_PSRAM
    printf("\n******** Test case PSRAM ********\n");
    printf("* Reading:\n");

    printf("* Writing:\n");
    #endif

    #ifdef TEST_CASE_SD
    printf("\n******** Test case SD CARD ********\n");
    printf("* Reading:\n");

    printf("* Writing:\n");
    #endif
}
