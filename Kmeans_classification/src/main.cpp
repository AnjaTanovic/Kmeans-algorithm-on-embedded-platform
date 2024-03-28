/*
NOTES

-coor for clusters are uint8_t. See if it is ok
-in write and read Point funkcions, see if it ok made, especialy write (commaIndex etc.)
-for now, k, epochs, n etc. are macros


*/

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

#define TRAIN_NUM 60000		//number of training images
#define TEST_NUM 10000		//number of test images
#define DIM 784				//number of dimensions

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
	
uint16_t n = 0; 			//number of points (images)
uint8_t num_per_file = 16;	//number of points stored in file
/*
Points are stored in files of 16 points per file, so
on SD are 60000/16 = 3750 files for training, and
10000/16 = 625 files for testing
*/

char csv_train[] = "/mnist_train_images/img";
char csv_test[] = "/mnist_test_images/img";
const char csv_result_path[] = "/training_results/result";
char csv_result[50]; 

uint8_t label_clust[K]; 	//meaning of cluster (number)

/*
struct Image 
{
	double coor[dim];	// coordinates
	int cluster;		// nearest cluster, no default cluster
	int label;			// label from csv file
	double minDist;  	// distance to nearest cluster, default infinite

	Image()
	{
		for (int i = 0; i < dim; i++)
		{
			coor[i] = 0.0;
		}
		label = -1;
		cluster = -1;
		minDist = __DBL_MAX__; 
	}
        
	Image(double *coordinates, int lab, int clust, double minDistance)
	{ 
		for (int i = 0; i < dim; i++)
		{
			coor[i] = coordinates[i];
		}
		label = lab;
		cluster = clust;
		minDist = minDistance; 
	}
};
*/

/*
//POINTS arrays
uint8_t imgCoor[TRAIN_NUM][DIM];		// coordinates for every image (2d array)
uint16_t imgCluster[TRAIN_NUM];			// nearest cluster for every image, no default cluster
uint8_t imgLabel[TRAIN_NUM];			// label from csv file for every image
int imgMinDist[TRAIN_NUM];				// distance to nearest cluster for every image, default infinite
*/

//CENTROID arrays
uint8_t cntrCoor[K][DIM];				// coordinates for every image (2d array) 
uint16_t cntrCluster[K];				// nearest cluster for every image, no default cluster
uint8_t cntrLabel[K];					// label from csv file for every image

//Variable image point
uint8_t varImgCoor[DIM];				// coordinates for image 
uint16_t varImgCluster;					// nearest cluster for variable image, no default cluster
uint8_t varImgLabel;					// label from csv file for variable image
int varImgMinDist;						// distance to nearest cluster for variable image, default infinite

//Arrays for centroid computation
uint16_t nPoints[K]; 					//array for each cluster, to sum number of points for particular cluster
double sum[DIM][K];  					//sum all coordinates for each cluster, to find center
uint8_t previous_coor[DIM];				//used for computing new centroids

int distance(uint8_t *coor1, uint8_t *coor2) {
	int dist = 0;
	for (uint16_t i = 0; i < DIM; i++) {
		dist += (coor1[i] - coor2[i]) * (coor1[i] - coor2[i]);
	}
    return dist;
}

void readPoint(fs::FS &fs, char * path, uint16_t file_number, uint8_t imgNumber, uint8_t *coor, uint16_t *cluster, uint8_t *label, bool imgFlag) {

	//Point is always read in the same variables for image (varImgCoor[DIM], varImgCluster, varImgLabel, varImgMinDist)
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

    while (file.available() && currentLine < imgNumber) {
        line = file.readStringUntil('\n');

        // Increment the line counter
        currentLine++;

        if (currentLine == imgNumber) {
			//Read line
            commaIndex = line.indexOf(',', start);
            *label = (uint8_t)(line.substring(start, commaIndex).toInt());
            start = commaIndex + 1;

            for (uint16_t i = 0; i <= DIM; i++) {
                commaIndex = line.indexOf(',', start);
                coor[i] = (uint8_t)(line.substring(start, commaIndex).toInt());
                start = commaIndex + 1;
            }

			commaIndex = line.indexOf(',', start);
			*cluster = (uint16_t)(line.substring(start, commaIndex).toInt());
			start = commaIndex + 1;
            commaIndex = line.indexOf(',', start);
			if (imgFlag)
				varImgMinDist = line.substring(start, commaIndex).toInt();

            return;
        }
    }
	file.close();
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
}

void writePoint(fs::FS &fs, char * path, uint16_t file_number, uint8_t imgNumber, const uint16_t cluster, const int minDist) {

	char new_path[50]; 
	sprintf(new_path, "%s_%d.csv", path, (int)file_number);


	File file = fs.open(new_path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

	// Reset file position to beginning
    file.seek(0);

    uint8_t currentLine = 0;
    uint32_t start = 0;
    uint32_t commaIndex;

    String line;

	while (file.available() && currentLine < imgNumber) {
        line = file.readStringUntil('\n');

        // Increment the line counter
        currentLine++;

        if (currentLine == imgNumber) {
			//Update line

			//Skip label
            commaIndex = line.indexOf(',', start);
            start = commaIndex + 1;

			//Skip coordinates
            for (uint16_t i = 0; i <= DIM; i++) {
                commaIndex = line.indexOf(',', start);
                start = commaIndex + 1;
            }

			commaIndex = line.indexOf(',', start);
			file.seek(commaIndex + 1);
			file.print(cluster);
			file.print(',');
			file.print(minDist);
			file.print('\n');

            return;
        }
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

	uint16_t num_of_files = n/num_per_file;


	for (uint16_t i = 0; i < K; i++) {
		//init centroids (random)
		readPoint(SD_MMC, csv_train, (uint16_t) (rand() % num_of_files), (uint8_t) (rand() % num_per_file), cntrCoor[i], &cntrCluster[i], &cntrLabel[i], 0);
	}
	#ifdef DEBUG
		Serial.println("Centroids initialized");
	#endif

	int dist; 					//used for distance function

	bool changed = true;

	uint16_t file_iterator = 0;
	
	for (uint16_t iter = 0; iter < EPOCHS; iter++)
	{
		changed = false;
		
		for (uint16_t c = 0; c < K; c++) 
		{
			for (uint16_t i = 0; i < n; i++) 
			{
				if (i % 16 == 0 && i != 0)
				file_iterator++;

				//load image number i
				readPoint(SD_MMC, csv_train, file_iterator, i % 16, varImgCoor, &varImgCluster, &varImgLabel, 1); 

			    dist = distance(cntrCoor[c], varImgCoor);
			    if (dist < varImgMinDist) 
			    {
					//varImgMinDist = dist;
					//varImgCluster = c;
					writePoint(SD_MMC, csv_train, file_iterator, i % 16, c, dist); 
			    }
			}
		}

		#ifdef DEBUG
			Serial.println("Distances calculated");
		#endif

		// Initialise nPoints and sum with zeroes
		for (uint16_t j = 0; j < K; ++j) 
		{
			nPoints[j] = 0;
			for (uint16_t i = 0; i < DIM; i++)
			{
				sum[i][j] = 0.0;
			}
		}
		
		file_iterator = 0;
		// Iterate over points to append data to centroids
		for (uint16_t i = 0; i < n; i++) 
		{
			if (i % 16 == 0 && i != 0)
				file_iterator++;

			readPoint(SD_MMC, csv_train, file_iterator, i % 16, varImgCoor, &varImgCluster, &varImgLabel, 1); 
			nPoints[varImgCluster] += 1;
			for (int i = 0; i < DIM; i++)
			{
				sum[i][varImgCluster] += varImgCoor[i];
			}
		
			//point.minDist = __DBL_MAX__;  // reset distance
			writePoint(SD_MMC, csv_train, file_iterator, i % 16, varImgCluster, __INT_MAX__); 
		}
		#ifdef DEBUG
			Serial.println("Sum 2d array for centroids computed");
		#endif

		// Compute the new centroids using sum arrays
		for (uint16_t c = 0; c < K; c++) 
		{
			for (uint16_t i = 0; i < DIM; i++)
			{
				previous_coor[i] = cntrCoor[c][i];
				cntrCoor[c][i] = sum[i][c] / nPoints[c];
				if (previous_coor[i] != cntrCoor[c][i])
					changed = true;
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
	uint16_t file_iterator = 0;

	for (uint16_t i = 0; i < K; i++)
	{
		//Initialization of label numbers
		for (uint16_t j = 0; j < K; j++)
		{
			label_num[j] = 0;
		}

		file_iterator = 0;
		//Counting labels for cluster i	
		for (uint16_t j = 0; j < n; j++)
		{
			if (j % 16 == 0 && j != 0)
				file_iterator++;
			readPoint(SD_MMC, csv_train, file_iterator, j % 16, varImgCoor, &varImgCluster, &varImgLabel, 1); 
			
			if (varImgCluster == i)
				label_num[varImgLabel] += 1; 
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

uint8_t predict()
{
	//uint8_t lab;
	int dist;
	
	for (uint16_t c = 0; c < K; c++) 
	{
		//lab = label_clust[c];
		dist = distance(cntrCoor[c], varImgCoor);
		if (dist < varImgMinDist)
		{
			varImgMinDist = dist;
			//varImgLabel = lab;
			varImgLabel = label_clust[c]; //real label of cluster c
		}
	}

	return varImgLabel;
}

double calculateTrainingAccuracy()
{
	uint16_t total_labels = n; 		//all points have label
	uint16_t correct_labels = 0;  	//points with correct classification
	uint16_t file_iterator = 0;

	for (int i = 0; i < n; i++)
	{
		if (i % 16 == 0 && i != 0)
			file_iterator++;

		readPoint(SD_MMC, csv_train, file_iterator, i % 16, varImgCoor, &varImgCluster, &varImgLabel, 1); 
		
		if (varImgLabel == label_clust[varImgCluster])
		{
			correct_labels++;
		}
	}
	return (double)correct_labels/(double)total_labels;
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

	for (uint16_t i = 0; i < n; i++) 
	{
		if (i % 16 == 0 && i != 0)
			file_iterator++;

		readPoint(SD_MMC, csv_test, file_iterator, i % 16, varImgCoor, &varImgCluster, &varImgLabel, 1);
		original_number = varImgLabel;
		predicted_number = predict();
		if (predicted_number == original_number)
			correct_labels++;
	}
	
	accuracy = (double)correct_labels/(double)n; //n is now number of points in test set

	Serial.print("* Accuracy for test set is ");
  	Serial.print(100*accuracy);
  	Serial.println("%."); 
	
	/*
	//Store results
	ofstream myfile;
	sprintf(csv_result, "%s_k%d.csv", csv_result_path, k);
	myfile.open(csv_result);
	
	int j = 0;	
	for (vector<Image>::iterator c = begin(centroids); c != end(centroids); ++c) 
	{
		myfile << label_clust[j];
		for (int i = 0; i < dim; i++)
			myfile << "," << c->coor[i];
		myfile << endl;
		
		j++;
	}
	*/
	/* Uncomment for different type of report 
	for (vector<Image>::iterator it = begin(test_points); it != end(test_points); ++it) 
	{
		myfile << it->label << "," << label_clust[it->cluster] << endl;
	}
	*/
		
	//myfile.close();
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