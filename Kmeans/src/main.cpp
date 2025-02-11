#include <Arduino.h>

#include <ctime>     
#include <fstream> 
#include <iostream>  
#include <sstream> 
#include <vector>
#include <cmath>
#include <time.h>

#include "FS.h"
#include "SD_MMC.h"

#include "SPIFFS.h"

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

using namespace std;
	
uint16_t n = 0; 					//number of points (used for both training and testing)
const uint8_t num_of_points_per_file = 250;	//number of points stored in file
//const uint16_t num_of_bytes_per_point = 1 + DIM + 1 + 4;  	//number of bytes per point (1 for label, 
															//DIM for img pixels, 1 for cluster, 
															//4 for min Dist) 
/*
Points are stored in files of 250 points per file, so
on SD are 60000/250 = 240 files for training, and
10000/250 = 40 files for testing
*/

char bin_train[] = "/mnist_train_images/img";
char bin_test[] = "/mnist_test_images/img";
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
uint8_t varImgCoor[num_of_points_per_file][DIM];				// coordinates for image 
uint8_t varImgCluster[num_of_points_per_file];				// nearest cluster for variable image, no default cluster
uint8_t varImgLabel[num_of_points_per_file];					// label from csv file for variable image
int varImgMinDist[num_of_points_per_file];					// distance to nearest cluster for variable image, default infinite

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
	uint16_t num_of_files = n/num_of_points_per_file;

	char new_path[50]; 
	sprintf(new_path, "%s_%d.bin", path, (int)(rand() % num_of_files));

	#ifdef DEBUG
	Serial.print("Random point ");
	Serial.println(new_path);
	#endif

	uint8_t imgNumber = rand() % num_of_points_per_file;

	#ifdef DATASET_ON_FLASH
	File file = SPIFFS.open(new_path);
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }
	#else
	File file = SD_MMC.open(new_path);
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }
	#endif

	//iterate over 1 file with 'num_of_points_per_file' points, and find random point
	for (uint16_t currentPoint = 0; currentPoint < num_of_points_per_file; currentPoint++) {
		if (currentPoint == imgNumber) {
				//read label
				cntrLabel[number_of_centroid] = (uint8_t)file.read();

				//read coordinates
				for (uint16_t i = 0; i < DIM; i++) {
					cntrCoor[number_of_centroid][i] = (uint8_t)file.read();
				}

				//read cluster
				cntrCluster[number_of_centroid] = (uint8_t)file.read();
			break;
		}
		else {
			//dummy read current point
			for (int i = 0; i < (1+DIM+1+4); i++)
				file.read();
		}
	}

	file.close();
}

void readPoints(char * path, uint16_t file_number) {

	//Points are always read in the same variables for image (varImgCoor[num_of_points_per_file][DIM], varImgCluster[num_of_points_per_file], 
	//varImgLabel[num_of_points_per_file], varImgMinDist[num_of_points_per_file])
	char new_path[50]; 
	sprintf(new_path, "%s_%d.bin", path, (int)file_number);

	#ifdef DATASET_ON_FLASH
	File file = SPIFFS.open(new_path);
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }
	#else
	File file = SD_MMC.open(new_path);
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }
	#endif

	//iterate over 1 file with 'num_of_points_per_file' points
	for (uint16_t currentPoint = 0; currentPoint < num_of_points_per_file; currentPoint++) {
			//read label
			varImgLabel[currentPoint] = (uint8_t)file.read();

			//read coordinates
			/*
			for (uint16_t i = 0; i < DIM; i++) {
				varImgCoor[currentPoint][i] = (uint8_t)file.read();
			}
			*/
			file.readBytes((char *)varImgCoor[currentPoint], DIM);

			//read cluster
			varImgCluster[currentPoint] = (uint8_t)file.read();

			//read minDist (4 bytes)
			varImgMinDist[currentPoint] = 0;
			for (int8_t i = 3; i >= 0; i--) {
				uint8_t byteRead = (uint8_t)file.read();
				varImgMinDist[currentPoint] |= (byteRead << (i * 8));
			}
	}

	file.close();
}

void writePoints(char * path, uint16_t file_number) {

	char new_path[50]; 
	sprintf(new_path, "%s_%d.bin", path, (int)file_number);

	#ifdef DATASET_ON_FLASH
	File file = SPIFFS.open(new_path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
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
    file.seek(0);

	//iterate over 1 file with 'num_of_points_per_file' points
	for (uint16_t currentPoint = 0; currentPoint < num_of_points_per_file; currentPoint++) {
			//write label
			file.write(varImgLabel[currentPoint]);

			//write coordinates
			for (uint16_t i = 0; i < DIM; i++) {
				file.write(varImgCoor[currentPoint][i]);
			}

			//write cluster
			file.write(varImgCluster[currentPoint]);

			//write minDist (4 bytes)
			file.write((uint8_t)(varImgMinDist[currentPoint] >> 24)); // Write the most significant byte
			file.write((uint8_t)(varImgMinDist[currentPoint] >> 16)); // Write the next most significant byte
			file.write((uint8_t)(varImgMinDist[currentPoint] >> 8));  // Write the next least significant byte
			file.write((uint8_t)varImgMinDist[currentPoint]);  
	}

	file.close();
}

void printFile(char * path, uint16_t file_number) {

	char new_path[50]; 
	sprintf(new_path, "%s_%d.csv", path, (int)file_number);

    #ifdef DATASET_ON_FLASH
	File file = SPIFFS.open(new_path);
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }
	#else
	File file = SD_MMC.open(new_path);
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }
	#endif

    Serial.printf("Reading file: %s\n", new_path);
    while(file.available()){
        Serial.write(file.read());
    }
	file.close();
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
		//load first 'num_of_points_per_file' images
		readPoints(bin_train, file_iterator);
		//Counting labels for cluster i	
		for (uint16_t j = 0; j < n; j++)
		{
			file_point_iterator = j % num_of_points_per_file;	
			if (file_point_iterator == 0 && j != 0) {
				file_iterator++;
				//load next 'num_of_points_per_file' images
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

	//load first 'num_of_points_per_file' images
	readPoints(bin_train, file_iterator); 
	for (int i = 0; i < n; i++)
	{
		file_point_iterator = i % num_of_points_per_file;
		if (file_point_iterator == 0 && i != 0) {
			file_iterator++;
			//load next 'num_of_points_per_file' images
			readPoints(bin_train, file_iterator); 
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

void kMeansClustering()
{
	#ifdef DEBUG
		Serial.print("K-means algorithm started. K = ");
		Serial.println(K);
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
		Serial.println("Centroids initialized");
	#endif

	int dist; 					//used for distance function
	uint8_t clusterId;			//used for sum and nPoints arrays
	sum = new uint16_t*[DIM];
	for (int i = 0; i < DIM; ++i) {
        sum[i] = new uint16_t[K];
    }

	bool changed = true;

	uint16_t file_iterator;			//define which file is now using 
	uint8_t file_point_iterator;	//define on which image is now computing 
	bool update = false;			//define whether file should be updated
	
	for (uint16_t iter = 0; iter < EPOCHS; iter++)
	{
		changed = false;
		
		for (uint8_t c = 0; c < K; c++) 
		{
			#ifdef DEBUG
				Serial.print("Calculating for centroid number ");
				Serial.println(c);
			#endif

			file_iterator = 0;
			//load first 'num_of_points_per_file' images
			readPoints(bin_train, file_iterator);
			for (uint16_t i = 0; i < n; i++) 
			{
				file_point_iterator = i % num_of_points_per_file;
				if (file_point_iterator == 0 && i != 0) {

					if (update) 
						writePoints(bin_train, file_iterator); 

					update = false;
					file_iterator++;

					//load next 'num_of_points_per_file' images
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
			//update last 'num_of_points_per_file' points
			if (update)
				writePoints(bin_train, file_iterator); 
		}

		#ifdef DEBUG
			Serial.println("Distances calculated");
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
			Serial.println("Arrays nPoints and sum initialized");
		#endif

		// Iterate over points to append data to centroids
		file_iterator = 0;
		//load first 'num_of_points_per_file' images
		readPoints(bin_train, file_iterator);
		
		for (uint16_t i = 0; i < n; i++) 
		{
			file_point_iterator = i % num_of_points_per_file;	
			if (file_point_iterator == 0 && i != 0) {
				writePoints(bin_train, file_iterator); 

				file_iterator++;

				//load next 'num_of_points_per_file' images
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
		//update last 'num_of_points_per_file' points
		writePoints(bin_train, file_iterator); 

		#ifdef DEBUG
			Serial.println("Sum 2d array for centroids computed");
		#endif

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
		Serial.print("Finished iteration number ");
    	Serial.print(iter);
    	Serial.println(".");
		#endif
		
		if (changed == false)    //Check if kmeans algorithm has converged
		{
			Serial.print("Kmeans algorithm has converged. Total number of iterations: ");
      		Serial.println(iter);
			break;  
		}
		if (iter == EPOCHS - 1)
		{
			Serial.print("Kmeans algorithm has reached maximum number of iterations, but not converged. Total number of iterations: ");
      		Serial.println(iter + 1);
			changed = false; //ending for loop
			break;
		}

		#ifdef CALC_ACC_BETWEEN_ITERATIONS
		assignLabelToCluster();
		//Calculate accuracy between true labels and kmeans labels, every iteration
		double accuracy = calculateTrainingAccuracy();
		
		Serial.print("* Accuracy for training set is ");
		Serial.print(100*accuracy);
		Serial.println("%."); 
		#endif
	}
	
}

void mainProgram()
{
	/*
	Serial.print("Insert k: ");
	while(!Serial.available());
	delay(100); //longer delay if using VS code monitor
	k = Serial.parseInt();
	Serial.println(k);

  	Serial.print("Insert maximum number of iterations: ");
	while(!Serial.available());
  	delay(100);
  	ep = Serial.parseInt();
  	Serial.println(ep);
	*/

	Serial.println("TRAINING STARTED.");

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

	Serial.println("Training finished.");
	Serial.print("Time spent: ");
	Serial.print(time_taken);
	Serial.println(" s.");
	
	//Calculate accuracy between true labels and kmeans labels
	double accuracy = calculateTrainingAccuracy();
	
	Serial.print("* Accuracy for training set is ");
	Serial.print(100*accuracy);
	Serial.println("%."); 

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
		file_point_iterator = i % num_of_points_per_file;
		if (file_point_iterator == 0 && i != 0) {
			#ifdef DEBUG
			Serial.print("Testing for file ");
			Serial.println(file_iterator);
			#endif
			
			file_iterator++;

			//load next 'num_of_points_per_file' images
			readPoints(bin_test, file_iterator);
		}

		
		original_number = varImgLabel[file_point_iterator];
		predicted_number = predict(file_point_iterator);
		if (predicted_number == original_number)
			correct_labels++;
	}
	
	accuracy = (double)correct_labels/(double)n; //n is now number of points in test set

	Serial.print("* Accuracy for test set is ");
  	Serial.print(100*accuracy);
  	Serial.println("%."); 
	#endif
	
	//Store results
	#ifdef DEBUG
	Serial.println("Storing results...");
	#endif
	
	sprintf(csv_result, "%s_k%d.csv", csv_result_path, K); 

	#ifdef DATASET_ON_FLASH
	File file = SPIFFS.open(csv_result, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
	#else
	File file = SD_MMC.open(csv_result, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
	#endif
	
	// Reset file position to beginning (FILE_WRITE opens at the end of the file)
    file.seek(0);

	for (uint8_t c = 0; c < K; c++) {

		//Write label
		file.print(label_clust[c]);

		//Write coordinates
		for (uint16_t i = 0; i < DIM; i++) {
			file.print(',');
			file.print(cntrCoor[c][i]);
		}
		file.print('\n');
    }
	file.close();

	#ifdef DEBUG
	#ifdef DATASET_ON_FLASH
	file = SPIFFS.open(csv_result, FILE_READ);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }
	#else
	file = SD_MMC.open(csv_result, FILE_READ);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }
	#endif

    Serial.printf("Reading result file: %s\n", csv_result);
    while(file.available()){
        Serial.write(file.read());
    }
	file.close();
	#endif

	for (int i = 0; i < DIM; ++i) {
        delete[] sum[i];
    }
    delete[] sum;
}

void setup() {

  	Serial.begin(115200);
  	delay(3000); //wait for opening monitor (Board ESP32 Dev Module, 115200 baudrate)
  
  	Serial.println("Hello from esp32!");


	#ifdef DATASET_ON_FLASH
	//Init file system
	if(!SPIFFS.begin(true)){
		Serial.println("An Error has occurred while mounting SPIFFS");
		return;
	}
	#else
	//Init SD card
  	if(!SD_MMC.begin()){
		Serial.println("Card Mount Failed");
		return;
	}
	uint8_t cardType = SD_MMC.cardType();

	if(cardType == CARD_NONE){
		Serial.println("No SD_MMC card attached");
		return;
	}

	/*
	Serial.print("SD_MMC Card Type: ");
	if(cardType == CARD_MMC){
		Serial.println("MMC");
	} else if(cardType == CARD_SD){
		Serial.println("SDSC");
	} else if(cardType == CARD_SDHC){
		Serial.println("SDHC");
	} else {
		Serial.println("UNKNOWN");
	}
	*/

	uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
	#endif

	mainProgram();
}

void loop() {

}