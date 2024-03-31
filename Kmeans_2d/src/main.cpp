#include <Arduino.h>
#include "SPIFFS.h"

#include <ctime>     
#include <fstream> 
#include <iostream>  
#include <sstream> 
#include <vector>

using namespace std;

int n = 0; //number of points

/*
const int sizeOfSet = 20;
double initialPoints[sizeOfSet][2] = {{1,7},
                                      {2,6},
                                      {3,2},
                                      {4,5},
                                      {6,1},
                                      {6,4},
                                      {6,6},
                                      {8,2},
                                      {8,5},
                                      {9,1},
                                      {1,3},
                                      {2,6},
                                      {30,2},
                                      {4,50},
                                      {6,1},
                                      {63,4},
                                      {6,46},
                                      {8,32},
                                      {80,15},
                                      {94,21}};
*/

struct Point 
{
    double x, y;     // coordinates
    int cluster;     // nearest cluster, no default cluster
    double minDist;  // distance to nearest cluster, default infinite

    Point() : 
        x(0.0), 
        y(0.0),
        cluster(-1),
        minDist(__DBL_MAX__) {}
        
    Point(double x, double y) : 
        x(x), 
        y(y),
        cluster(-1),
        minDist(__DBL_MAX__) {}

    double distance(Point p) {
        return (p.x - x) * (p.x - x) + (p.y - y) * (p.y - y);
    }
};

void kMeansClustering(vector<Point>* points, int epochs, int k)
{
	vector<Point> centroids;
	srand(time(0));  // need to set the random seed
	for (int i = 0; i < k; ++i) {
    	centroids.push_back(points->at(rand() % n));
	}
	
	for (int iter = 0; iter < epochs; iter++)
	{
		for (vector<Point>::iterator c = begin(centroids); c != end(centroids); ++c) {
			// quick hack to get cluster index
			int clusterId = c - begin(centroids);

			for (vector<Point>::iterator it = points->begin();
			     it != points->end(); ++it) {
				 
			    Point p = *it;
			    double dist = c->distance(p);
			    if (dist < p.minDist) {
            p.minDist = dist;
            p.cluster = clusterId;
			    }
			    *it = p;
			}
		}
		
		vector<int> nPoints;
		vector<double> sumX, sumY;

		// Initialise with zeroes
		for (int j = 0; j < k; ++j) {
			nPoints.push_back(0);
			sumX.push_back(0.0);
			sumY.push_back(0.0);
		}
		
		// Iterate over points to append data to centroids
		for (vector<Point>::iterator it = points->begin(); 
			 it != points->end(); ++it) {
			int clusterId = it->cluster;
			nPoints[clusterId] += 1;
			sumX[clusterId] += it->x;
			sumY[clusterId] += it->y;

			it->minDist = __DBL_MAX__;  // reset distance
		}

		// Compute the new centroids
		for (vector<Point>::iterator c = begin(centroids); 
			 c != end(centroids); ++c) {
			int clusterId = c - begin(centroids);
			c->x = sumX[clusterId] / nPoints[clusterId];
			c->y = sumY[clusterId] / nPoints[clusterId];
		}
	}
}

void printFile(String fileName) {
  File file = SPIFFS.open(fileName);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  
  //Serial.println("File Content:");
  while(file.available()){
    Serial.write(file.read());
  }
  Serial.println();

  file.close();
}

vector<Point> readPoints() 
{
    /* 
    //Using initialPoints array 
    vector<Point> points;

    while (n < sizeOfSet) {
        double x = initialPoints[n][0];
        double y = initialPoints[n][1];
        points.push_back(Point(x, y));
        n++;
    }
    return points;
    */

    vector<Point> points;

    File file = SPIFFS.open("/input.txt", FILE_READ);
    if(!file){
      Serial.println("Failed to open file for reading");
      return points;
    }

    string line;
    while(file.available()){
      String line = file.readStringUntil('\n');
      int commaIndex = line.indexOf(',');

      if(commaIndex != -1){
        String xString = line.substring(0, commaIndex);
        String yString = line.substring(commaIndex + 1);

        points.push_back(Point(xString.toInt(), yString.toInt()));
        n++;
      }
    }
    file.close();
    return points;
}

void mainProgram()
{
	int k;				//number of clusters
	Serial.print("Insert k: ");
	while(!Serial.available());
  delay(100); //longer delay if using VS code monitor
  k = Serial.parseInt();
  Serial.println(k);

	int ep; 			//number of iterations
  Serial.print("Insert number of iterations: ");
	while(!Serial.available());
  delay(100);
  ep = Serial.parseInt();
  Serial.println(ep);

  Serial.println("*****************************************");
  Serial.println("Kmeans algorithm is running");
  Serial.println("*****************************************");

	vector<Point> points = readPoints(); 
	kMeansClustering(&points, ep, k); // pass address of points to function
	
  /*
  //Print clusters on monitor
  Serial.println("x    y    cluster");
	for (vector<Point>::iterator it = points.begin(); it != points.end(); ++it) {
		Serial.print(it->x);
    Serial.print("    ");
    Serial.print(it->y);
    Serial.print("    ");
    Serial.println(it->cluster);
	}
  */

  //Save clusters in output file
  File outputFile = SPIFFS.open("/output.txt", FILE_WRITE);
  if(!outputFile){
    Serial.println("There was an error opening the file for writing");
    return;
  }
  
  for (vector<Point>::iterator it = points.begin(); it != points.end(); ++it) {
    outputFile.print(it->x);
    outputFile.print(",");
    outputFile.print(it->y);
    outputFile.print(",");
    outputFile.println(it->cluster);
  }
  outputFile.close();
  
}

void setup() {

  Serial.begin(115200);
  delay(3000); //wait for opening monitor
  
  Serial.println("Hello from esp32!");

  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  //Check file content 
  Serial.println("Input file:");
  printFile("/input.txt");

  mainProgram();

  //Check file content 
  Serial.println("Output file:");
  printFile("/output.txt");
}

void loop() {

}