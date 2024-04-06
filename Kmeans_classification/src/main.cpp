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

#define DEBUG
#define TEST_SEED

#define TRAIN_NUM 1000		//number of training images
#define TEST_NUM 1000		//number of test images
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
const uint8_t num_per_file = 50;	//number of points stored in file
/*
Points are stored in files of 50 points per file, so
on SD are 60000/50 = 1200 files for training, and
10000/16 = 20 files for testing
*/

char csv_train[] = "/mnist_train_images/img";
char csv_test[] = "/mnist_test_images/img";
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
uint16_t cntrCluster[K];				// nearest cluster for every image, no default cluster (default is -1, which is 65535)
uint8_t cntrLabel[K];					// label from csv file for every image

//Variable image point
uint8_t varImgCoor[num_per_file][DIM];				// coordinates for image 
uint16_t varImgCluster[num_per_file];				// nearest cluster for variable image, no default cluster
uint8_t varImgLabel[num_per_file];					// label from csv file for variable image
int varImgMinDist[num_per_file];					// distance to nearest cluster for variable image, default infinite

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

void readPoints(fs::FS &fs, char * path, uint16_t file_number) {

	//Points are always read in the same variables for image (varImgCoor[num_per_file][DIM], varImgCluster[num_per_file], 
	//varImgLabel[num_per_file], varImgMinDist[num_per_file])
	char new_path[50]; 
	sprintf(new_path, "%s_%d.csv", path, (int)file_number);

	File file = fs.open(new_path);
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }

	uint8_t currentLine = 0;
    uint32_t start = 0;
    uint32_t commaIndex;

	String line; 

    while (file.available()) {
        line = file.readStringUntil('\n');

		//Read line
		start = 0;
		commaIndex = line.indexOf(',', start);
		varImgLabel[currentLine] = (uint8_t)(line.substring(start, commaIndex).toInt());
		start = commaIndex + 1;

		for (uint16_t i = 0; i < DIM; i++) {
			commaIndex = line.indexOf(',', start);
			varImgCoor[currentLine][i] = (uint8_t)(line.substring(start, commaIndex).toInt());
			start = commaIndex + 1;
		}

		commaIndex = line.indexOf(',', start);
		varImgCluster[currentLine] = (uint16_t)(line.substring(start, commaIndex).toInt());
		start = commaIndex + 1;

		//commaIndex = line.indexOf('\n', start);
		varImgMinDist[currentLine] = line.substring(start, line.length()).toInt();

		// Increment the line counter
        currentLine++;
    }
	file.close();
}

void readRandomPoint(fs::FS &fs, char * path, uint16_t number_of_centroid) {

	//Read random point from random file
	uint16_t num_of_files = n/num_per_file;

	char new_path[50]; 
	sprintf(new_path, "%s_%d.csv", path, (int)(rand() % num_of_files));

	#ifdef DEBUG
	Serial.print("Random point ");
	Serial.println(new_path);
	#endif

	uint8_t imgNumber = rand() % num_per_file;

	File file = fs.open(new_path);
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }

	uint8_t currentLine = 0;
    uint32_t start = 0;
    uint32_t commaIndex;

	String line; 

    while (file.available()) {
        line = file.readStringUntil('\n');
		if (currentLine == imgNumber) {
			
			//Read line
			commaIndex = line.indexOf(',', start);
			cntrLabel[number_of_centroid] = (uint8_t)(line.substring(start, commaIndex).toInt());
			start = commaIndex + 1;

			for (uint16_t i = 0; i < DIM; i++) {
				commaIndex = line.indexOf(',', start);
				cntrCoor[number_of_centroid][i] = (uint8_t)(line.substring(start, commaIndex).toInt());
				start = commaIndex + 1;
			}

			commaIndex = line.indexOf(',', start);
			cntrCluster[number_of_centroid] = (uint16_t)(line.substring(start, commaIndex).toInt());
	
			break;
		}

		// Increment the line counter
        currentLine++;
    }
	file.close();
}

void printFile(fs::FS &fs, char * path, uint16_t file_number) {

	char new_path[50]; 
	sprintf(new_path, "%s_%d.csv", path, (int)file_number);

    File file = fs.open(new_path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

	
    Serial.printf("Reading file: %s\n", new_path);
    while(file.available()){
        Serial.write(file.read());
    }
	file.close();
}

void writePoints(fs::FS &fs, char * path, uint16_t file_number) {

	char new_path[50]; 
	sprintf(new_path, "%s_%d.csv", path, (int)file_number);

	File file = fs.open(new_path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
			
	// Reset file position to beginning (FILE_WRITE opens at the end of the file)
    file.seek(0);

	for (uint16_t currentLine = 0; currentLine < num_per_file; currentLine++) {

		//Write label
		file.print(varImgLabel[currentLine]);
		file.print(',');

		//Write coordinates
		for (uint16_t i = 0; i < DIM; i++) {
			file.print(varImgCoor[currentLine][i]);
			file.print(',');
		}

		//Write cluster
		file.print(varImgCluster[currentLine]);
		file.print(',');

		//Write min distance
		file.print(varImgMinDist[currentLine]);
		file.print('\n');
    }
	file.close();
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

	for (uint16_t i = 0; i < K; i++) {
		//init centroids (random)
		readRandomPoint(SD_MMC, csv_train, i);
	}
	#ifdef DEBUG
		Serial.println("Centroids initialized");
	#endif

	int dist; 					//used for distance function
	uint16_t clusterId;			//used for sum and nPoints arrays
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
		
		for (uint16_t c = 0; c < K; c++) 
		{
			#ifdef DEBUG
				Serial.print("Calculating for centroid number ");
				Serial.println(c);
			#endif

			file_iterator = 0;
			//load first 'num_per_file' images
			readPoints(SD_MMC, csv_train, file_iterator);
			for (uint16_t i = 0; i < n; i++) 
			{
				file_point_iterator = i % num_per_file;
				if (file_point_iterator == 0 && i != 0) {
					#ifdef DEBUG
					//printFile(SD_MMC, csv_train, file_iterator); 
					#endif

					if (update) 
						writePoints(SD_MMC, csv_train, file_iterator); 

					update = false;
					file_iterator++;

					//load next 'num_per_file' images
					readPoints(SD_MMC, csv_train, file_iterator); 
				}

			    dist = distance(cntrCoor[c], varImgCoor[file_point_iterator]);
			    if (dist < varImgMinDist[file_point_iterator]) 
			    {
					varImgMinDist[file_point_iterator] = dist;
					varImgCluster[file_point_iterator] = c;
					update = true;
			    }
			}
			//update last 'num_per_file' points
			if (update)
				writePoints(SD_MMC, csv_train, file_iterator); 
		}

		#ifdef DEBUG
			Serial.println("Distances calculated");
		#endif

		// Initialize nPoints and sum with zeroes
		for (uint16_t j = 0; j < K; ++j) 
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
		//load first 'num_per_file' images
		readPoints(SD_MMC, csv_train, file_iterator);
		for (uint16_t i = 0; i < n; i++) 
		{
			file_point_iterator = i % num_per_file;	
			if (file_point_iterator == 0 && i != 0) {
				writePoints(SD_MMC, csv_train, file_iterator); 

				file_iterator++;

				//load next 'num_per_file' images
				readPoints(SD_MMC, csv_train, file_iterator); 
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
		//update last 'num_per_file' points
		writePoints(SD_MMC, csv_train, file_iterator); 

		#ifdef DEBUG
			Serial.println("Sum 2d array for centroids computed");
		#endif

		// Compute the new centroids using sum arrays
		for (uint16_t c = 0; c < K; c++) 
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
	}
	
}

void assignLabelToCluster()
{
	uint16_t label_num[K];   //contains the number of label occurence in one cluster
	uint16_t file_iterator;
	uint8_t file_point_iterator;	

	for (uint16_t i = 0; i < K; i++)
	{
		//Initialization of label numbers
		for (uint16_t j = 0; j < K; j++)
		{
			label_num[j] = 0;
		}

		file_iterator = 0;
		//load first 'num_per_file' images
		readPoints(SD_MMC, csv_train, file_iterator);
		//Counting labels for cluster i	
		for (uint16_t j = 0; j < n; j++)
		{
			file_point_iterator = j % num_per_file;	
			if (file_point_iterator == 0 && j != 0) {
				file_iterator++;
				//load next 'num_per_file' images
				readPoints(SD_MMC, csv_train, file_iterator);
			}
			
			if (varImgCluster[file_point_iterator] == i)
				label_num[varImgLabel[file_point_iterator]] += 1; 
		}
		
		//The most numerous label becomes a cluster reprezentation number from range (1,10)
		uint16_t max = label_num[0];
		label_clust[i] = 0;
		for (uint16_t j = 0; j < K; j++)
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

	//load first 'num_per_file' images
	readPoints(SD_MMC, csv_train, file_iterator); 
	for (int i = 0; i < n; i++)
	{
		file_point_iterator = i % num_per_file;
		if (file_point_iterator == 0 && i != 0) {
			file_iterator++;
			//load next 'num_per_file' images
			readPoints(SD_MMC, csv_train, file_iterator); 
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
	
	for (uint16_t c = 0; c < K; c++) 
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

	//Testing
	n = TEST_NUM;
	uint8_t predicted_number;
	uint8_t original_number;
	uint16_t correct_labels = 0;

	uint16_t file_iterator = 0;
	uint8_t file_point_iterator;
	readPoints(SD_MMC, csv_test, file_iterator);
	for (uint16_t i = 0; i < n; i++) 
	{
		file_point_iterator = i % num_per_file;
		if (file_point_iterator == 0 && i != 0) {
			#ifdef DEBUG
			Serial.print("Testing for file ");
			Serial.println(file_iterator);
			#endif
			
			file_iterator++;

			//load next 'num_per_file' images
			readPoints(SD_MMC, csv_test, file_iterator);
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
	
	//Store results
	#ifdef DEBUG
	Serial.println("Storing results...");
	#endif
	
	sprintf(csv_result, "%s_k%d.csv", csv_result_path, K); 

	File file = SD_MMC.open(csv_result, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
	// Reset file position to beginning (FILE_WRITE opens at the end of the file)
    file.seek(0);

	for (uint16_t c = 0; c < K; c++) {

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
	file = SD_MMC.open(csv_result, FILE_READ);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

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

	mainProgram();
}

void loop() {

}