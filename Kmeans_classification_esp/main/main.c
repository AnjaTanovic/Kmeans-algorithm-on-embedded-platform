#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

#include <time.h>

//#include "SD_MMC.h"

#include "esp_spiffs.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "FileSystem";

#define DEBUG
#define TEST_SEED
#define CALC_ACC_BETWEEN_ITERATIONS

#define DATASET_ON_FLASH

#define TRAIN_NUM 1000		//number of training images
#define TEST_NUM 10000		//number of test images
#define DIM 196				//number of dimensions

#define K 10
/*
K is number of clusters (	1.option - defined as macro,
							2.option - defined as max, for example 500
							3.option not defined, arrays are created dynamically)
*/
#define EPOCHS 200
/*
EPOCHS is max number of iterations (1.option - defined as macro,
									2.option - defined as max, for example 500
									3.option not defined, created dynamically)
*/
	
uint16_t n = 0; 					//number of points (used for both training and testing)
#define NUM_OF_POINTS_PER_FILE 250	//number of points stored in file
//const uint16_t num_of_bytes_per_point = 1 + DIM + 1;  	//number of bytes per point (1 for label, 
															//DIM for img pixels, 1 for cluster) 
/*
Points are stored in files of 250 points per file, so
on SD are 60000/250 = 240 files for training, and
10000/250 = 40 files for testing
*/

char bin_train[] = "/img";
char bin_test[] = "/test/img";
const char csv_result_path[] = "/result";
char csv_result[50]; 

uint8_t label_clust[K]; 	//meaning of cluster (number)

/*
//POINTS arrays
uint8_t imgCoor[TRAIN_NUM][DIM];		// coordinates for every image (2d array)
uint16_t imgCluster[TRAIN_NUM];			// nearest cluster for every image, no default cluster
uint8_t imgLabel[TRAIN_NUM];			// label from csv file for every image
int imgMinDist[TRAIN_NUM];				// distance to nearest cluster for every image, default infinite
*/

//CENTROID arrays
uint8_t cntrCoor[K][DIM];				// coordinates for every image (2d array) 
uint8_t cntrCluster[K];				// nearest cluster for every image, no default cluster (default is -1, which is 65535)
uint8_t cntrLabel[K];					// label from csv file for every image

//Variable image point
uint8_t varImgCoor[NUM_OF_POINTS_PER_FILE][DIM];				// coordinates for image 
uint8_t varImgCluster[NUM_OF_POINTS_PER_FILE];				// nearest cluster for variable image, no default cluster
uint8_t varImgLabel[NUM_OF_POINTS_PER_FILE];					// label from csv file for variable image

//Arrays for centroid computation
uint16_t nPoints[K]; 					//array for each cluster, to sum number of points for particular cluster
uint16_t **sum; 
//double sum[DIM][K];  					//sum all coordinates for each cluster, to find center
uint8_t previous_coor[DIM];				//used for computing new centroids

int distance(uint8_t *coor1, uint8_t *coor2) {
	int dist = 0;
	for (uint16_t i = 0; i < DIM; i++) {
		dist += (coor1[i] - coor2[i]) * (coor1[i] - coor2[i]);
	}
    return dist;
}

void readRandomPoint(char * path, uint8_t number_of_centroid) {

	//Read random point from random file
	uint16_t num_of_files = n/NUM_OF_POINTS_PER_FILE;

	char new_path[50]; 
	sprintf(new_path, "/spiffs%s_%d.bin", path, (int)(rand() % num_of_files));

	uint8_t imgNumber = rand() % NUM_OF_POINTS_PER_FILE;
	
	#ifdef DEBUG
	printf("Random point ");
	printf("%s, point %d\n", new_path, imgNumber);
	#endif

	#ifdef DATASET_ON_FLASH
	FILE* f = fopen(new_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
	}
	#else
	File file = SD_MMC.open(new_path);
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }
	#endif

	//iterate over 1 file with 'NUM_OF_POINTS_PER_FILE' points, and find random point
	for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
		if (currentPoint == imgNumber) {
				//read label
				fread(&cntrLabel[number_of_centroid], 1, 1, f);

				//read coordinates
				fread(cntrCoor[number_of_centroid], 1, DIM, f);

				//read cluster
				fread(&cntrCluster[number_of_centroid], 1, 1, f);

			break;
		}
		else {
			// Seek ahead in the file by a certain number of bytes (whole point)
			fseek(f, 1 + DIM + 1, SEEK_CUR);
		}
	}

	fclose(f);
}

void readPoints(char * path, uint16_t file_number) {

	//Points are always read in the same variables for image (varImgCoor[NUM_OF_POINTS_PER_FILE][DIM], varImgCluster[NUM_OF_POINTS_PER_FILE], 
	//varImgLabel[NUM_OF_POINTS_PER_FILE])
	char new_path[50]; 
	sprintf(new_path, "/spiffs%s_%d.bin", path, (int)file_number);

	#ifdef DATASET_ON_FLASH
	FILE* f = fopen(new_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
	#else
	File file = SD_MMC.open(new_path);
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }
	#endif

	//iterate over 1 file with 'NUM_OF_POINTS_PER_FILE' points
	for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
			//read label
			fread(&varImgLabel[currentPoint], 1, 1, f);

			//read coordinates
			fread(varImgCoor[currentPoint], 1, DIM, f);

			//read cluster
			fread(&varImgCluster[currentPoint], 1, 1, f);

		/*	if (calc) {
			printf("Point %d: ", currentPoint);
			printf("%d ", varImgLabel[currentPoint]);
			for (int i = 0 ; i < DIM; i++)
				printf("%d ", varImgCoor[currentPoint][i]);
			printf("%d ", varImgCluster[currentPoint]);
			}*/
	}

	fclose(f);
}

void writePoints(char * path, uint16_t file_number) {

	char new_path[50]; 
	sprintf(new_path, "/spiffs%s_%d.bin", path, (int)file_number);

	#ifdef DATASET_ON_FLASH
	FILE* f = fopen(new_path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
	#else
	File file = SD_MMC.open(new_path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
	#endif
			
	// Reset file position to beginning (FILE_WRITE opens at the end of the file)
    fseek(f, 0, SEEK_SET);

	//iterate over 1 file with 'NUM_OF_POINTS_PER_FILE' points
	for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
			//write label
			fwrite(&varImgLabel[currentPoint], 1, 1, f);

			//write coordinates
			fwrite(varImgCoor[currentPoint], 1, DIM, f);

			//write cluster
			fwrite(&varImgCluster[currentPoint], 1, 1, f);

			/*
			fwrite(writeData, 1, DIM+2, f);  // Write all bytes of point at once
			*/
	}

	fclose(f);
}

void assignLabelToCluster()
{
	uint16_t label_num[K];   //contains the number of label occurence in one cluster
	uint16_t file_iterator;
	uint8_t file_point_iterator;	

	for (uint8_t i = 0; i < K; i++)
	{
		//Initialization of label numbers
		for (uint8_t j = 0; j < K; j++)
		{
			label_num[j] = 0;
		}

		file_iterator = 0;
		//load first 'NUM_OF_POINTS_PER_FILE' images
		readPoints(bin_train, file_iterator);
		//Counting labels for cluster i	
		for (uint16_t j = 0; j < n; j++)
		{
			file_point_iterator = j % NUM_OF_POINTS_PER_FILE;	
			if (file_point_iterator == 0 && j != 0) {
				file_iterator++;
				//load next 'NUM_OF_POINTS_PER_FILE' images
				readPoints(bin_train, file_iterator);
			}
			
			if (varImgCluster[file_point_iterator] == i)
				label_num[varImgLabel[file_point_iterator]] += 1; 
		}

		//The most numerous label becomes a cluster reprezentation number from range (1,10)
		uint16_t max = label_num[0];
		label_clust[i] = 0;
		for (uint8_t j = 0; j < K; j++)
		{
			if (label_num[j] > max)
			{
				max = label_num[j];
				label_clust[i] = j;
			}
		}
	}
}

double calculateTrainingAccuracy()
{
	uint16_t total_labels = n; 		//all points have label
	uint16_t correct_labels = 0;  	//points with correct classification
	uint16_t file_iterator = 0;
	uint8_t file_point_iterator;

	//load first 'NUM_OF_POINTS_PER_FILE' images
	readPoints(bin_train, file_iterator); 
	for (int i = 0; i < n; i++)
	{
		file_point_iterator = i % NUM_OF_POINTS_PER_FILE;
		if (file_point_iterator == 0 && i != 0) {
			file_iterator++;
			//load next 'NUM_OF_POINTS_PER_FILE' images
			readPoints(bin_train, file_iterator); 
		}

		if (varImgLabel[file_point_iterator] == label_clust[varImgCluster[file_point_iterator]])
		{
			correct_labels++;
		}
	}
	return (double)correct_labels/(double)total_labels;
}

/*
uint8_t predict(uint8_t point)
{
	//uint8_t lab;
	int dist;
	
	for (uint8_t c = 0; c < K; c++) 
	{
		//lab = label_clust[c];
		dist = distance(cntrCoor[c], varImgCoor[point]);
		if (dist < varImgMinDist[point])
		{
			varImgMinDist[point] = dist;
			//varImgLabel = lab;
			varImgLabel[point] = label_clust[c]; //real label of cluster c
		}
	}

	return varImgLabel[point];
}
*/

void kMeansClustering()
{
	#ifdef DEBUG
		printf("K-means algorithm started. K = ");
		printf("%d\n", K);
	#endif

	#ifdef TEST_SEED
	srand(0); 	  //set always the same seed for testing
	#else
	srand(time(0));  //set the random seed
	#endif

	for (uint8_t i = 0; i < K; i++) {
		//init centroids (random)
		readRandomPoint(bin_train, i);
	}
	#ifdef DEBUG
		printf("Centroids initialized\n");
	#endif

	int dist; 					//used for distance function
	int minDist;				//used for tracking minimal distance for current point
	uint8_t clusterId;			//used for sum and nPoints arrays

	// Allocate memory for the array of pointers
	sum = (uint16_t **)malloc(DIM * sizeof(uint16_t *));
    if (sum == NULL) {
        // Handle memory allocation failure
		printf("Not enough memory for sum array\n");
        return;
    }
    // Allocate memory for each row (array of uint16_t)
    for (int i = 0; i < DIM; i++) {
        sum[i] = (uint16_t *)malloc(K * sizeof(uint16_t));
        if (sum[i] == NULL) {
            // Handle memory allocation failure
			printf("Not enough memory for sum array\n");
            return;
        }
    }
	
	bool changed = true;

	uint16_t file_iterator;			//define which file is now using 
	uint8_t file_point_iterator;	//define on which image is now computing 
	
	for (uint16_t iter = 0; iter < EPOCHS; iter++)
	{
		changed = false;

		//New code version
		//for each image calculate the closest cluster and sum it for new centroids
		//this approach reduces reading and writing to files

		// Initialize nPoints and sum with zeroes
		for (uint8_t j = 0; j < K; ++j) 
		{
			nPoints[j] = 0;
			for (uint16_t i = 0; i < DIM; i++)
			{
				sum[i][j] = 0.0;
			}
		}

		file_iterator = 0;
		//load first 'NUM_OF_POINTS_PER_FILE' images
		readPoints(bin_train, file_iterator);
		for (uint16_t i = 0; i < n; i++) {

			//track number of point inside each file
			file_point_iterator = i % NUM_OF_POINTS_PER_FILE;
			if (file_point_iterator == 0 && i != 0) {

				writePoints(bin_train, file_iterator); 
				
				file_iterator++;

				//load next 'NUM_OF_POINTS_PER_FILE' images
				readPoints(bin_train, file_iterator); 
			}

			minDist = __INT_MAX__;
			//find the closest cluster
			for (uint8_t c = 0; c < K; c++) {
				dist = distance(cntrCoor[c], varImgCoor[file_point_iterator]);
			    if (dist < minDist) 
			    {
					minDist = dist;
					varImgCluster[file_point_iterator] = c;
			    }
			}

			clusterId = varImgCluster[file_iterator];
			nPoints[clusterId] += 1;
			for (int coor = 0; coor < DIM; coor++)
			{
				sum[coor][clusterId] += varImgCoor[file_point_iterator][coor];
			}
		}
		writePoints(bin_train, file_iterator); 
		
/*
		//Old code version
		for (uint8_t c = 0; c < K; c++) 
		{
			#ifdef DEBUG
				printf("Calculating for centroid number ");
				printf("%d\n", c);
			#endif

			file_iterator = 0;
			//load first 'NUM_OF_POINTS_PER_FILE' images
			readPoints(bin_train, file_iterator);
			for (uint16_t i = 0; i < n; i++) 
			{
				file_point_iterator = i % NUM_OF_POINTS_PER_FILE;
				if (file_point_iterator == 0 && i != 0) {

					if (update) {
						writePoints(bin_train, file_iterator); 
					}

					update = false;
					file_iterator++;

					//load next 'NUM_OF_POINTS_PER_FILE' images
					readPoints(bin_train, file_iterator); 
				}

			    dist = distance(cntrCoor[c], varImgCoor[file_point_iterator]);
			    if (dist < varImgMinDist[file_point_iterator]) 
			    {
					varImgMinDist[file_point_iterator] = dist;
					varImgCluster[file_point_iterator] = c;
					update = true;
			    }
			}
			//update last 'NUM_OF_POINTS_PER_FILE' points
			if (update) {
				writePoints(bin_train, file_iterator); 
				update = false;
			}

		}

		#ifdef DEBUG
			printf("Distances calculated\n");
		#endif

		// Initialize nPoints and sum with zeroes
		for (uint8_t j = 0; j < K; ++j) 
		{
			nPoints[j] = 0;
			for (uint16_t i = 0; i < DIM; i++)
			{
				sum[i][j] = 0.0;
			}
		}
		
		#ifdef DEBUG
			printf("Arrays nPoints and sum initialized\n");
		#endif

		// Iterate over points to append data to centroids
		file_iterator = 0;
		//load first 'NUM_OF_POINTS_PER_FILE' images
		readPoints(bin_train, file_iterator);
		
		for (uint16_t i = 0; i < n; i++) 
		{
			file_point_iterator = i % NUM_OF_POINTS_PER_FILE;	
			if (file_point_iterator == 0 && i != 0) {
				writePoints(bin_train, file_iterator); 

				file_iterator++;

				//load next 'NUM_OF_POINTS_PER_FILE' images
				readPoints(bin_train, file_iterator); 
			}

			clusterId = varImgCluster[file_point_iterator];
			nPoints[clusterId] += 1;
			for (int i = 0; i < DIM; i++)
			{
				sum[i][clusterId] += varImgCoor[file_point_iterator][i];
			}
			//point.minDist = __INT_MAX__;  // reset distance
			varImgMinDist[file_point_iterator] = __INT_MAX__;
		}
		//update last 'NUM_OF_POINTS_PER_FILE' points
		writePoints(bin_train, file_iterator); 

		#ifdef DEBUG
			printf("Sum 2d array for centroids computed\n");
		#endif
*/

		// Compute the new centroids using sum arrays
		for (uint8_t c = 0; c < K; c++) 
		{
			for (uint16_t i = 0; i < DIM; i++)
			{
				previous_coor[i] = cntrCoor[c][i];
				if (nPoints[c] != 0) {
					cntrCoor[c][i] = sum[i][c] / nPoints[c];
					if (previous_coor[i] != cntrCoor[c][i])
						changed = true;
				}
			}
		}
		
		#ifdef DEBUG
		printf("Finished iteration number ");
    	printf("%d\n", iter);
		#endif
		
		if (changed == false)    //Check if kmeans algorithm has converged
		{
			printf("Kmeans algorithm has converged. Total number of iterations: ");
      		printf("%d\n",iter);
			break;  
		}
		if (iter == EPOCHS - 1)
		{
			printf("Kmeans algorithm has reached maximum number of iterations, but not converged. Total number of iterations: ");
      		printf("%d\n", iter + 1);
			changed = false; //ending for loop
			break;
		}

		#ifdef CALC_ACC_BETWEEN_ITERATIONS
		assignLabelToCluster();
		//Calculate accuracy between true labels and kmeans labels, every iteration
		double accuracy = calculateTrainingAccuracy();
		
		printf("* Accuracy for training set is ");
		printf("%lf", 100*accuracy);
		printf("%%.\n"); 
		#endif
	}
	
}

void app_main(void)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Hello from esp32!\n");

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

    #ifdef DATASET_ON_FLASH
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
	#else
	//Init SD card
  	if(!SD_MMC.begin()){
		printf("Card Mount Failed\n");
		return;
	}
	uint8_t cardType = SD_MMC.cardType();

	if(cardType == CARD_NONE){
		printf("No SD_MMC card attached\n");
		return;
	}

	uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    printf("SD_MMC Card Size: %lluMB\n", cardSize);
	#endif

    printf("TRAINING STARTED.\n");

	n = TRAIN_NUM;
	
	//Start measuring time
	clock_t t;
   	t = clock();	

	//Training
	//Execute k-means algorithm
	kMeansClustering(); 

    //Calculate which cluster is which number (result is in label_clust array)
	assignLabelToCluster();
	
	//Finish measuring time
	t = clock() - t;
	double time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds

	printf("Training finished.\n");
	printf("Time spent: ");
	printf("%lf", time_taken);
	printf(" s.\n");
	
	//Calculate accuracy between true labels and kmeans labels
	double accuracy = calculateTrainingAccuracy();
	
	printf("* Accuracy for training set is ");
	printf("%lf", 100*accuracy);
	printf("%%.\n"); 

	#ifndef DATASET_ON_FLASH
	//Testing
	n = TEST_NUM;
	uint8_t predicted_number;
	uint8_t original_number;
	uint16_t correct_labels = 0;

	uint16_t file_iterator = 0;
	uint8_t file_point_iterator;
	readPoints(bin_test, file_iterator);
	for (uint16_t i = 0; i < n; i++) 
	{
		file_point_iterator = i % NUM_OF_POINTS_PER_FILE;
		if (file_point_iterator == 0 && i != 0) {
			#ifdef DEBUG
			printf("Testing for file ");
			printf("%d", file_iterator);
			#endif
			
			file_iterator++;

			//load next 'NUM_OF_POINTS_PER_FILE' images
			readPoints(bin_test, file_iterator);
		}

		
		original_number = varImgLabel[file_point_iterator];
		predicted_number = predict(file_point_iterator);
		if (predicted_number == original_number)
			correct_labels++;
	}
	
	accuracy = (double)correct_labels/(double)n; //n is now number of points in test set

	printf("* Accuracy for test set is \n");
  	printf("%lf", 100*accuracy);
  	printf("%%.\n"); 
	#endif
	
	//Store results
	#ifdef DEBUG
	printf("Storing results...\n");
	#endif
	
	#ifdef DATASET_ON_FLASH
	sprintf(csv_result, "/spiffs%s_k%d.csv", csv_result_path, K); 

	FILE* f = fopen(csv_result, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
	#else
	sprintf(csv_result, "%s_k%d.csv", csv_result_path, K); 

	File file = SD_MMC.open(csv_result, FILE_WRITE);
    if(!file){
        printf("Failed to open file for writing\n");
        return;
    }
	#endif
	
	// Reset file position to beginning (FILE_WRITE opens at the end of the file)
    //file.seek(0);

	for (uint8_t c = 0; c < K; c++) {

		//Write label
		fwrite(&label_clust[c], 1, 1, f);

		//Write coordinates
		fwrite(cntrCoor[c], 1, DIM, f);
		fprintf(f, "\n");
    }
	fclose(f);

	#ifdef DEBUG
	#ifdef DATASET_ON_FLASH
	f = fopen(csv_result, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
	#else
	file = SD_MMC.open(csv_result, FILE_READ);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }
	#endif

    printf("Reading result file: %s\n", csv_result);
	int byte;
	while ((byte = fgetc(f)) != EOF) {
        // Process each byte (here we just print it)
        printf("%02X\n", byte);
    }
	fclose(f);
	#endif

	// Free memory
    for (int i = 0; i < DIM; i++) {
        free(sum[i]); // Free memory for each row
    }
    free(sum); // Free memory for the array of pointers

    #ifdef DATASET_ON_FLASH
    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");
    #endif
}
