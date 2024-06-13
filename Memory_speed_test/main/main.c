#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

#include <esp_timer.h>

#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_log.h"

#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"

/*****************************************************************************************************************************************/
//CPU frequency = 240 MHz (Component config -> ESP System Settings)
//Flash SPI speed = 80 MHz, Flash size = 4 MB (Serial flasher config)
//PSRAM clock speed = 80 MHz (Component config -> ESP PSRAM -> Support -> SPI RAM config -> Set RAM clock speed)
//PSRAM static arrays in PSRAM (Component config -> ESP PSRAM -> Support -> SPI RAM config -> allow .bss segment placed in external memory)
/*****************************************************************************************************************************************/

#define TEST_CASE_FLASH
#define TEST_CASE_PSRAM
#define TEST_CASE_SD

#ifdef TEST_CASE_FLASH
static const char *TAG_flash = "FileSystem";
#endif

#ifdef TEST_CASE_SD
static const char *TAG_sd = "SD";
#endif

//ESP32 uses a two-way set-associative cache. Each of the two CPUs has 32 KB of cache featuring a block
//size of 32 bytes for accessing external storage

#define DIM 196
#define NUM_OF_POINTS_PER_FILE 250

uint8_t dataCoor[NUM_OF_POINTS_PER_FILE][DIM];				
uint8_t dataCluster[NUM_OF_POINTS_PER_FILE];			
uint8_t dataLabel[NUM_OF_POINTS_PER_FILE];

#ifdef TEST_CASE_PSRAM
EXT_RAM_BSS_ATTR uint8_t readData[NUM_OF_POINTS_PER_FILE*(DIM+2)];	//array in external RAM (PSRAM) where dataset is stored			
#endif

char readPath[30] = "/read.bin";     //img_0.bin from mnist dataset (250 images)
char writePath[30] = "/write.bin";   //empty file

void testFLASH() {
    //Variables for time measuring
    uint64_t start_time, end_time;

    printf("\n******** Test case FLASH ********\n");

    //Init file system
    ESP_LOGI(TAG_flash, "Initializing SPIFFS");

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
            ESP_LOGE(TAG_flash, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG_flash, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG_flash, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_flash, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    } else {
        ESP_LOGI(TAG_flash, "Partition size: total: %d, used: %d", total, used);
    }

    // Check consistency of reported partition size info.
    if (used > total) {
        ESP_LOGW(TAG_flash, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_flash, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return;
        } else {
            ESP_LOGI(TAG_flash, "SPIFFS_check() successful");
        }
    }

    printf("* Reading:\n");
    
    char new_read_path_flash[50]; 
	sprintf(new_read_path_flash, "/spiffs%s", readPath);
    FILE* f;

    //Start measuring time
    start_time = esp_timer_get_time();
        
    f = fopen(new_read_path_flash, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG_flash, "Failed to open file for reading");
        return;
    }
    for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
        //read label
        fread(&dataLabel[currentPoint], 1, 1, f);

        //read coordinates
        fread(dataCoor[currentPoint], 1, DIM, f);

        //read cluster
        fread(&dataCluster[currentPoint], 1, 1, f);
	}
    fclose(f); 

    //Finish measuring time
    end_time = esp_timer_get_time();
    printf("Time spent for reading: %llu microseconds\n\n", end_time - start_time); 
         
    printf("* Writing:\n");

    char new_write_path_flash[50]; 
	sprintf(new_write_path_flash, "/spiffs%s", writePath);

    //Start measuring time
    start_time = esp_timer_get_time();        

    f = fopen(new_write_path_flash, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG_flash, "Failed to open file for writing");
        return;
    }
    for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
        //write label
        fwrite(&dataLabel[currentPoint], 1, 1, f);

        //write coordinates
        fwrite(dataCoor[currentPoint], 1, DIM, f);

        //write cluster
        fwrite(&dataCluster[currentPoint], 1, 1, f);
	}
    fclose(f);

    //Finish measuring time
    end_time = esp_timer_get_time();
    printf("Time spent for writing: %llu microseconds\n\n", end_time - start_time);    

    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG_flash, "SPIFFS unmounted");
}

void testPSRAM() {
    //Variables for time measuring
    uint64_t start_time, end_time;

    printf("\n******** Test case PSRAM ********\n");

    //Fill array in PSRAM with data from image
    for (uint16_t i = 0; i < NUM_OF_POINTS_PER_FILE*(DIM+2); i++)
        readData[i] = i;

    printf("* Reading:\n");

   	//Start measuring time
    start_time = esp_timer_get_time();
        
    for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
        //read label
        dataLabel[currentPoint] = readData[currentPoint*(DIM+2)];

        //read coordinates
        for (uint16_t coor = 0; coor < DIM; coor++)
            dataCoor[currentPoint][coor] = readData[currentPoint*(DIM+2) + coor + 1];

        //read cluster
        dataCluster[currentPoint] = readData[currentPoint*(DIM+2) + DIM + 1];
	}

    //Finish measuring time
    end_time = esp_timer_get_time();
    printf("Time spent for reading: %llu microseconds\n\n", end_time - start_time); 
         
    printf("* Writing:\n");

    //Start measuring time
    start_time = esp_timer_get_time();	
        
    for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
        //write label
        readData[currentPoint*(DIM+2)] = currentPoint;

        //write coordinates
        for (uint16_t coor = 0; coor < DIM; coor++)
            readData[currentPoint*(DIM+2) + coor + 1] = currentPoint;

        //write cluster
        readData[currentPoint*(DIM+2) + DIM + 1] = currentPoint;
	}

    //Finish measuring time
    end_time = esp_timer_get_time();
    printf("Time spent for writing:: %llu microseconds\n\n", end_time - start_time); 
}

void testSD() {
    //Variables for time measuring
    uint64_t start_time, end_time;

    printf("\n******** Test case SD ********\n");

    //Init sd card
    ESP_LOGI(TAG_sd, "Initializing SD card");

    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5
    };
 
    ESP_LOGI(TAG_sd, "Mounting filesystem");
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG_sd, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG_sd, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG_sd, "Filesystem mounted");

    printf("* Reading:\n");

    char new_read_path_sd[50]; 
	sprintf(new_read_path_sd, "/sdcard%s", readPath);
    FILE* f;

    //Start measuring time
    start_time = esp_timer_get_time();
        
    f = fopen(new_read_path_sd, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG_flash, "Failed to open file for reading");
        return;
    }   
    for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
        //read label
        fread(&dataLabel[currentPoint], 1, 1, f);

        //read coordinates
        fread(dataCoor[currentPoint], 1, DIM, f);

        //read cluster
        fread(&dataCluster[currentPoint], 1, 1, f);
	}
    fclose(f); 

    //Finish measuring time
    end_time = esp_timer_get_time();
    printf("Time spent for reading: %llu microseconds\n\n", end_time - start_time); 
         
    printf("* Writing:\n");

    char new_write_path_sd[50]; 
	sprintf(new_write_path_sd, "/sdcard%s", writePath);

    //Start measuring time
    start_time = esp_timer_get_time();        

    f = fopen(new_write_path_sd, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG_sd, "Failed to open file for writing");
        return;
    }
    for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
        //write label
        fwrite(&dataLabel[currentPoint], 1, 1, f);

        //write coordinates
        fwrite(dataCoor[currentPoint], 1, DIM, f);

        //write cluster
        fwrite(&dataCluster[currentPoint], 1, 1, f);
	}
    fclose(f);

    //Finish measuring time
    end_time = esp_timer_get_time();
    printf("Time spent for writing: %llu microseconds\n\n", end_time - start_time);        

    // All done, unmount sd card
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG_sd, "Card unmounted");
}

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

    /*********************************** TEST CASES ***********************************/

    #ifdef TEST_CASE_FLASH
        testFLASH();
    #endif

    #ifdef TEST_CASE_PSRAM
        testPSRAM();
    #endif

    #ifdef TEST_CASE_SD
        testSD();
    #endif
}
