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

#include <string.h>

#define DEBUG
#define TEST_SEED
//#define CALC_ACC_BETWEEN_ITERATIONS
//#define PRINT_FILES

//Define only one case!!!
//#define DATASET_ON_FLASH
//#define DATASET_ON_SD
#define DATASET_IN_PSRAM	//dataset is initially on flash, and loaded in psram
							//results are stored on flash (for potential later usage)
							//flash is used because it saves data during sleep

#ifdef DATASET_ON_FLASH
static const char *TAG = "FileSystem";
#endif
#ifdef DATASET_ON_SD
static const char *TAG = "SD";
#endif
#ifdef DATASET_IN_PSRAM
static const char *TAG = "FileSystem";
#endif

#define TRAIN_NUM 1000		//number of training images
#define TEST_NUM 10000		//number of test images
#define DIM 196				//number of dimensions

#define K 200
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
#define NUM_OF_FILES 4				//number of files with data
#define NUM_OF_POINTS_PER_FILE 250	//number of points stored in file
//const uint16_t num_of_bytes_per_point = 1 + DIM + 1;  	//number of bytes per point (1 for label, 
															//DIM for img pixels, 1 for cluster) 
/*
Points are stored in files of 250 points per file, so
on SD are 60000/250 = 240 files for training, and
10000/250 = 40 files for testing
*/

#ifdef DATASET_ON_FLASH
char bin_train[] = "/img";
#endif
#ifdef DATASET_ON_SD
char bin_train[] = "/train/img";
char bin_test[] = "/test/img";
#endif
#ifdef DATASET_IN_PSRAM
char bin_train[] = "/img";
#endif

char result_path[] = "/result";
char result[50]; 

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

#ifdef DATASET_IN_PSRAM
//arrays in external RAM (PSRAM) where dataset is stored
EXT_RAM_BSS_ATTR uint8_t psramCoor[NUM_OF_FILES * NUM_OF_POINTS_PER_FILE][DIM];			
EXT_RAM_BSS_ATTR uint8_t psramCluster[NUM_OF_FILES * NUM_OF_POINTS_PER_FILE];
EXT_RAM_BSS_ATTR uint8_t psramLabel[NUM_OF_FILES * NUM_OF_POINTS_PER_FILE];					
#endif

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

#ifdef DATASET_IN_PSRAM
void loadRandomPoints() {
	uint16_t file_num;
	uint8_t imgNumber;
	for (uint8_t c = 0; c < K; c++) {
		file_num = rand() % NUM_OF_FILES;
		imgNumber = rand() % NUM_OF_POINTS_PER_FILE;
		#ifdef DEBUG
		printf("Random point from ");
		printf("group %d, point %d\n", file_num, imgNumber);
		#endif

		//load label	
		cntrLabel[c] = psramLabel[file_num * NUM_OF_POINTS_PER_FILE + imgNumber];

		//load coordinates
		/*
		for (uint8_t i = 0; i < DIM; i++)
			cntrCoor[c][i] = psramCoor[file_num * NUM_OF_POINTS_PER_FILE + imgNumber][i];
		*/
		memcpy(cntrCoor[c], psramCoor[file_num * NUM_OF_POINTS_PER_FILE + imgNumber], DIM);
		
		//load cluster
		cntrCluster[c] = psramCluster[file_num * NUM_OF_POINTS_PER_FILE + imgNumber];
	}
}
#else
void readRandomPoint(char * path, uint8_t number_of_centroid) {

	//Read random point from random file

	#ifdef DATASET_ON_FLASH
	char new_path[50]; 
	sprintf(new_path, "/spiffs%s_%d.bin", path, (int)(rand() % NUM_OF_FILES));
	#endif
	#ifdef DATASET_ON_SD
		char new_path[50]; 
	sprintf(new_path, "/sdcard%s_%d.bin", path, (int)(rand() % NUM_OF_FILES));
	#endif
	
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
	#endif
	#ifdef DATASET_ON_SD
	FILE* f = fopen(new_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
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
#endif

#ifdef DATASET_IN_PSRAM
void loadPoints(uint16_t file_number) {
	for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
		//load label
		varImgLabel[currentPoint] = psramLabel[file_number * NUM_OF_POINTS_PER_FILE + currentPoint];

		//load coordinates
		/*
		for (uint8_t i = 0; i < DIM; i++)
			varImgCoor[currentPoint][i] = psramCoor[file_num * NUM_OF_POINTS_PER_FILE + currentPoint][i];
		*/
		memcpy(varImgCoor[currentPoint], psramCoor[file_number * NUM_OF_POINTS_PER_FILE + currentPoint], DIM);
		
		//load cluster
		varImgCluster[currentPoint] = psramCluster[file_number * NUM_OF_POINTS_PER_FILE + currentPoint];
	}
}
#else
void readPoints(char * path, uint16_t file_number) {

	//Points are always read in the same variables for image (varImgCoor[NUM_OF_POINTS_PER_FILE][DIM], varImgCluster[NUM_OF_POINTS_PER_FILE], 
	//varImgLabel[NUM_OF_POINTS_PER_FILE])

	#ifdef DATASET_ON_FLASH
	char new_path[50]; 
	sprintf(new_path, "/spiffs%s_%d.bin", path, (int)file_number);
	FILE* f = fopen(new_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
	#endif
	#ifdef DATASET_ON_SD
	char new_path[50]; 
	sprintf(new_path, "/sdcard%s_%d.bin", path, (int)file_number);
	FILE* f = fopen(new_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
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

		#ifdef PRINT_FILES
		printf("Point %d from file %d:\n", currentPoint, file_number);
		printf("%d ", varImgLabel[currentPoint]);
		for (int i = 0 ; i < DIM; i++)
			printf("%d ", varImgCoor[currentPoint][i]);
		printf("%d \n\n", varImgCluster[currentPoint]);
		#endif
	}

	fclose(f);
}
#endif

#ifdef DATASET_IN_PSRAM
void storePoints(uint16_t file_number) {
	for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
		//store label
		psramLabel[file_number * NUM_OF_POINTS_PER_FILE + currentPoint] = varImgLabel[currentPoint];

		//store coordinates
		/*
		for (uint8_t i = 0; i < DIM; i++)
			psramCoor[file_num * NUM_OF_POINTS_PER_FILE + currentPoint][i] = varImgCoor[currentPoint][i];
		*/
		memcpy(psramCoor[file_number * NUM_OF_POINTS_PER_FILE + currentPoint], varImgCoor[currentPoint], DIM);
		
		//store cluster
		psramCluster[file_number * NUM_OF_POINTS_PER_FILE + currentPoint] = varImgCluster[currentPoint];
	}
}	
#else
void writePoints(char * path, uint16_t file_number) {

	#ifdef DATASET_ON_FLASH
	char new_path[50]; 
	sprintf(new_path, "/spiffs%s_%d.bin", path, (int)file_number);
	FILE* f = fopen(new_path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
	#endif
	#ifdef DATASET_ON_SD
	char new_path[50]; 
	sprintf(new_path, "/sdcard%s_%d.bin", path, (int)file_number);
	FILE* f = fopen(new_path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
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
#endif

void assignLabelToCluster()
{
	uint16_t **label_num;   //contains the number of label occurence for each cluster
							//2D array where the highest value in each column
							//corresponds to real label value for each cluster
							//Dimensions: [10][K]
									
	// Allocate memory for the array of pointers
	label_num = (uint16_t **)malloc(10 * sizeof(uint16_t *));
    if (label_num == NULL) {
        // Handle memory allocation failure
		printf("Not enough memory for label_num array\n");
        return;
    }
    // Allocate memory for each row (array of uint16_t)
    for (int i = 0; i < 10; i++) {
        label_num[i] = (uint16_t *)malloc(K * sizeof(uint16_t));
        if (label_num[i] == NULL) {
            // Handle memory allocation failure
			printf("Not enough memory for label_num array\n");
            return;
        }
    }

	uint16_t file_iterator;
	uint8_t file_point_iterator;	

	//Initialization of 2D array
	for (uint8_t i = 0; i < 10; i++)
	{
		for (uint8_t j = 0; j < K; j++)
			label_num[i][j] = 0;
	}

	file_iterator = 0;
	//load first 'NUM_OF_POINTS_PER_FILE' images
	#ifdef DATASET_IN_PSRAM
	loadPoints(file_iterator);
	#else
	readPoints(bin_train, file_iterator);
	#endif
	//Counting labels for clusters
	for (uint16_t j = 0; j < n; j++)
	{
		file_point_iterator = j % NUM_OF_POINTS_PER_FILE;	
		if (file_point_iterator == 0 && j != 0) {
			file_iterator++;
			//load next 'NUM_OF_POINTS_PER_FILE' images
			#ifdef DATASET_IN_PSRAM
			loadPoints(file_iterator);
			#else
			readPoints(bin_train, file_iterator);
			#endif
		}
		//fill 2D array
		label_num[varImgLabel[file_point_iterator]][varImgCluster[file_point_iterator]] += 1; 
	}

	//The most numerous label in each column becomes a cluster reprezentation number from range (0,9)
	//Results are in label_clust 1D array
	for (uint8_t col = 0; col < K; col++) {
		uint16_t max = label_num[0][col];
		label_clust[col] = 0;
		for (uint8_t row = 1; row < 10; row++) {
			if (label_num[row][col] > max)
			{
				max = label_num[row][col];
				label_clust[col] = row;
			}
		}
	}	

	// Free memory
    for (int i = 0; i < 10; i++) {
        free(label_num[i]); // Free memory for each row
    }
    free(label_num); // Free memory for the array of pointers
}

double calculateTrainingAccuracy()
{
	uint16_t total_labels = n; 		//all points have label
	uint16_t correct_labels = 0;  	//points with correct classification
	uint16_t file_iterator = 0;
	uint8_t file_point_iterator;

	//load first 'NUM_OF_POINTS_PER_FILE' images
	#ifdef DATASET_IN_PSRAM
	loadPoints(file_iterator);
	#else
	readPoints(bin_train, file_iterator);
	#endif 
	for (int i = 0; i < n; i++)
	{
		file_point_iterator = i % NUM_OF_POINTS_PER_FILE;
		if (file_point_iterator == 0 && i != 0) {
			file_iterator++;
			//load next 'NUM_OF_POINTS_PER_FILE' images
			#ifdef DATASET_IN_PSRAM
			loadPoints(file_iterator);
			#else
			readPoints(bin_train, file_iterator); 
			#endif
		}

		if (varImgLabel[file_point_iterator] == label_clust[varImgCluster[file_point_iterator]])
		{
			correct_labels++;
		}
	}
	return (double)correct_labels/(double)total_labels;
}

uint8_t predict(uint8_t point)
{
	//uint8_t lab;
	int dist;
	int minDist;

	minDist = __INT_MAX__;
	
	for (uint8_t c = 0; c < K; c++) 
	{
		//lab = label_clust[c];
		dist = distance(cntrCoor[c], varImgCoor[point]);
		if (dist < minDist)
		{
			minDist = dist;
			//varImgLabel = lab;
			varImgLabel[point] = label_clust[c]; //real label of cluster c
		}
	}

	return varImgLabel[point];
}

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

	#ifdef DATASET_IN_PSRAM
	loadRandomPoints();
	#else
	for (uint8_t i = 0; i < K; i++) {
		//init centroids (random)
		readRandomPoint(bin_train, i);
	}
	#endif
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

		// Yield control to other tasks (scheduler tasks, IDLE task...)
        vTaskDelay(10 / portTICK_PERIOD_MS);	//10 ms delay (default tick period is 10ms, so any delay of less than 10ms results in a 0 delay) 

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
		#ifdef DATASET_IN_PSRAM
		loadPoints(file_iterator);
		#else
		readPoints(bin_train, file_iterator);
		#endif
		for (uint16_t i = 0; i < n; i++) {

			//track number of point inside each file
			file_point_iterator = i % NUM_OF_POINTS_PER_FILE;
			if (file_point_iterator == 0 && i != 0) {
				#ifdef DATASET_IN_PSRAM
				storePoints(file_iterator);
				#else
				writePoints(bin_train, file_iterator); 
				#endif

				file_iterator++;

				//load next 'NUM_OF_POINTS_PER_FILE' images
				#ifdef DATASET_IN_PSRAM
				loadPoints(file_iterator);
				#else
				readPoints(bin_train, file_iterator); 
				#endif
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

			clusterId = varImgCluster[file_point_iterator];
			nPoints[clusterId] += 1;
			for (int coor = 0; coor < DIM; coor++)
			{
				sum[coor][clusterId] += varImgCoor[file_point_iterator][coor];
			}
		}
		#ifdef DATASET_IN_PSRAM
		storePoints(file_iterator);
		#else
		writePoints(bin_train, file_iterator); 
		#endif 
		
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
			#ifdef DEBUG
			printf("Kmeans algorithm has converged. Total number of iterations: ");
      		printf("%d\n",iter);
			#endif
			// Yield control to other tasks (scheduler tasks, IDLE task...)
        	vTaskDelay(10 / portTICK_PERIOD_MS);	//10 ms delay (default tick period is 10ms, so any delay of less than 10ms results in a 0 delay) 

			break;  
		}
		if (iter == EPOCHS - 1)
		{
			#ifdef DEBUG
			printf("Kmeans algorithm has reached maximum number of iterations, but not converged. Total number of iterations: ");
      		printf("%d\n", iter + 1);
			#endif
			changed = false; //ending for loop

			// Yield control to other tasks (scheduler tasks, IDLE task...)
        	vTaskDelay(10 / portTICK_PERIOD_MS);	//10 ms delay (default tick period is 10ms, so any delay of less than 10ms results in a 0 delay) 

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

	// Free memory
    for (int i = 0; i < DIM; i++) {
        free(sum[i]); // Free memory for each row
    }
    free(sum); // Free memory for the array of pointers
}

void app_main(void)
{
    vTaskDelay(3000 / portTICK_PERIOD_MS); //wait 3 seconds for opening the monitor

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
	#endif
	#ifdef DATASET_ON_SD
	//Init SD card
  	ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5
    };
 
    ESP_LOGI(TAG, "Mounting filesystem");
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");
	#endif
	#ifdef DATASET_IN_PSRAM
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
	#endif

	#ifdef DATASET_IN_PSRAM
	//Fill arrays in PSRAM with data from flash

	for (uint16_t file_num = 0; file_num < NUM_OF_FILES; file_num++) {
		char new_path[50]; 
		sprintf(new_path, "/spiffs%s_%d.bin", bin_train, file_num);
		FILE* f = fopen(new_path, "rb");
		if (f == NULL) {
			ESP_LOGE(TAG, "Failed to open file for reading");
			return;
		}

		//iterate over 1 file with 'NUM_OF_POINTS_PER_FILE' points
		for (uint16_t currentPoint = 0; currentPoint < NUM_OF_POINTS_PER_FILE; currentPoint++) {
			//read label
			fread(&psramLabel[(file_num * NUM_OF_POINTS_PER_FILE) + currentPoint], 1, 1, f);

			//read coordinates
			fread(psramCoor[(file_num * NUM_OF_POINTS_PER_FILE) + currentPoint], 1, DIM, f);

			//read cluster
			fread(&psramCluster[(file_num * NUM_OF_POINTS_PER_FILE) + currentPoint], 1, 1, f);

			#ifdef PRINT_FILES
			printf("Point %d from file %d:\n", currentPoint, file_num);
			printf("%d ", psramLabel[(file_num * NUM_OF_POINTS_PER_FILE) + currentPoint]);
			for (int i = 0 ; i < DIM; i++)
				printf("%d ", psramCoor[(file_num * NUM_OF_POINTS_PER_FILE) + currentPoint][i]);
			printf("%d \n\n", psramCluster[(file_num * NUM_OF_POINTS_PER_FILE) + currentPoint]);
			#endif
		}
		fclose(f);
	}
	#endif

    printf("TRAINING STARTED.\n");

	n = TRAIN_NUM;
	
	//Start measuring time
    uint64_t start_time = esp_timer_get_time();  

	//Training
	//Execute k-means algorithm
	kMeansClustering(); 

    //Calculate which cluster is which number (result is in label_clust array)
	assignLabelToCluster();
	
	//Finish measuring time
	uint64_t end_time = esp_timer_get_time();
	printf("Training finished.\n");
    printf("Time spent: %llu microseconds (%llu seconds).\n", end_time - start_time, (end_time - start_time)/1000000); 
	
	//Calculate accuracy between true labels and kmeans labels
	double accuracy = calculateTrainingAccuracy();
	
	printf("* Accuracy for training set is ");
	printf("%lf", 100*accuracy);
	printf("%%.\n"); 

	#ifdef DATASET_ON_SD
	//Testing
	printf("TESTING STARTED.\n");

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

	printf("* Accuracy for test set is ");
  	printf("%lf", 100*accuracy);
  	printf("%%.\n"); 
	#endif
	
	//Store results (final centroids)
	#ifdef DEBUG
	printf("Storing results...\n");
	#endif
	
	#ifdef DATASET_ON_FLASH
	sprintf(result, "/spiffs%s.bin", result_path); 
	#endif
	#ifdef DATASET_ON_SD
	sprintf(result, "/sdcard%s.bin", result_path); 
	#endif
	#ifdef DATASET_IN_PSRAM
	sprintf(result, "/spiffs%s.bin", result_path); 
	#endif

	FILE* f = fopen(result, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
	
	// Reset file position to beginning (FILE_WRITE opens at the end of the file)
    //file.seek(0);

	for (uint8_t c = 0; c < K; c++) {
		//Write label
		fwrite(&label_clust[c], 1, 1, f);

		//Write coordinates
		fwrite(cntrCoor[c], 1, DIM, f);
    }
	fclose(f);

	#ifdef DEBUG
	printf("Successfully stored centroids.\n");
	#endif

	#ifdef DEBUG
	#ifdef PRINT_FILES
	f = fopen(result, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    printf("Reading result file: %s\n", result);
	int byte;
	while ((byte = fgetc(f)) != EOF) {
        // Process each byte (here we just print it)
        printf("%02X\n", byte);
    }
	fclose(f);
	#endif
	#endif

    #ifdef DATASET_ON_FLASH
    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");
    #endif
	#ifdef DATASET_ON_SD
	// All done, unmount sd card
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");
	#endif
	#ifdef DATASET_IN_PSRAM
    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");
    #endif

	printf("Program finished.\n");
}
