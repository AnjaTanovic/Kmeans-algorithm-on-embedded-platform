#include <ctime>     
#include <fstream> 
#include <iostream>  
#include <sstream> 
#include <vector>
#include <cmath>
#include <time.h>
#include <omp.h>

//#define DEBUG 
#define TEST_SEED
#define PARAM_IN_ARGS

#define OMP_SCHEDULE dynamic

using namespace std;

const int dim = 784; //number of dimensions

char csv_result_path[] = "../training_results/result";
char csv_result[50]; 
	
int n = 0; //number of points (images)
int tc = 0; //number of threads
int ch_size = 0; //chunk size for dynamic or static shedule

int num_for_training = 0;

struct Image 
{
    double coor[dim];     // coordinates
    int cluster;     // no default cluster
    int label;	//label from csv file
    double minDist;  // default infinite dist to nearest cluster
   
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
        
    Image(double *coordinates, double lab)
    { 
    	for (int i = 0; i < dim; i++)
    	{
    		coor[i] = coordinates[i];
        }
        label = lab;
        cluster = -1;
        minDist = __DBL_MAX__; 
    }

    double distance(Image p) 
    {
    	int dist = 0;
    	for (int i = 0; i < dim; i++)
    	{
    		dist += (p.coor[i] - coor[i]) * (p.coor[i] - coor[i]);
    	}
        return dist;
    }
};

vector<Image> readCsv(char *csv_input) 
{
    vector<Image> points;
    string line;
    ifstream file(csv_input);

	getline(file, line); //read first line (info line)
	
	while (getline(file, line)) 
	{
		stringstream lineStream(line);
		string bit;
		double coordinates[dim];
		int lab;
		 
		getline(lineStream, bit, ',');
		lab = stof(bit);

		for (int i = 0; i < (dim - 1); i++)
		{
			getline(lineStream, bit, ',');
			coordinates[i] = stof(bit);
			//coordinates[i] /= 255; //Data normalization 
		}
		getline(lineStream, bit, '\n');
		coordinates[dim - 1] = stof(bit);

		points.push_back(Image(coordinates, lab));
		n++;
		
		if (n == num_for_training)
			break;
	}
    return points;
}

void kMeansClustering(vector<Image>* points, int epochs, int k, vector<Image>* centroids)
{
	#ifdef TEST_SEED
	srand(0); 	  //set always the same seed for testing
	#else
	srand(time(0));  //set the random seed
	#endif
	
	for (int i = 0; i < k; ++i) {
    		centroids->push_back(points->at(rand() % n));
	}
	
	bool changed = true;
	
	int centroidsSize = centroids->size();
	vector<Image>::iterator c_begin = centroids->begin();
	vector<Image>::iterator c;
	int pointsSize = points->size();
	vector<Image>::iterator it_begin = points->begin();
	vector<Image>::iterator it;
	
	for (int iter = 0; iter < epochs; iter++)
	{
		changed = false;

		for (int i = 0; i < centroidsSize; i++)
		{
			// get cluster index
			int clusterId = i;	
			c = c_begin + i;
			
			#pragma omp parallel for num_threads(tc) schedule(OMP_SCHEDULE,ch_size)
			for (int j = 0; j < pointsSize; j++)
			{
				it = it_begin + j;
			    Image p = *it;
			    double dist = c->distance(p);
			    if (dist < p.minDist) 
			    {
					p.minDist = dist;
					p.cluster = clusterId;
					(*points)[j].cluster = p.cluster;
			    	(*points)[j].minDist = p.minDist;
			    }
			    //*it = p;
			    
			}
		}
		
		/*
		#pragma omp parallel for num_threads(tc) schedule(OMP_SCHEDULE,ch_size) private(c)
		for (int j = 0; j < pointsSize; j++)
		{	
			for (int i = 0; i < centroidsSize; i++)
			{
				// get cluster index
				int clusterId = i;	
				c = c_begin + i;
				
				it = it_begin + j;
			    Image p = *it;
			    double dist = c->distance(p);
			    if (dist < p.minDist) 
			    {
					p.minDist = dist;
					p.cluster = clusterId;
					(*points)[j].cluster = p.cluster;
			    	(*points)[j].minDist = p.minDist;
			    }
			    //*it = p;
			    
			}
		}
		*/
		
		int nPoints[k];
		double sum[dim][k];

		// Initialise with zeroes
		#pragma omp parallel for num_threads(tc) schedule(static,1)
		for (int j = 0; j < k; ++j) 
		{
			nPoints[j] = 0;
			for (int i = 0; i < dim; i++)
			{
				sum[i][j] = 0.0;
			}
		}
		
		// Iterate over points to append data to centroids
		for (int j = 0; j < pointsSize; j++) 
		{
			it = it_begin + j;
			int clusterId = it->cluster;
			nPoints[clusterId] += 1;
			for (int i = 0; i < dim; i++)
			{
				sum[i][clusterId] += it->coor[i];
			}
		
			it->minDist = __DBL_MAX__;  // reset distance
		}

		/*		
		// Iterate over points to append data to centroids
		#pragma omp parallel for num_threads(tc) schedule(OMP_SCHEDULE,10)
		for (int clust = 0; clust < k; clust++)
		{
			for (int j = 0; j < pointsSize; j++)
			{
				it = it_begin + j;
				if (it->cluster == clust)
				{
					nPoints[clust] += 1;
					it->minDist = __DBL_MAX__;  // reset distance
					for (int i = 0; i < dim; i++)
					{
						sum[i][clust] += it->coor[i];
					}
				}
			}
		}
		*/

		// Compute the new centroids
		#pragma omp parallel for num_threads(tc) schedule(static,1)
		for (int j = 0; j < centroidsSize; j++) 
		{
			int clusterId = j;
			c = c_begin + j;
			
			double previous_coor[dim];
			for (int i = 0; i < dim; i++)
			{
				previous_coor[i] = c->coor[i];
				c->coor[i] = sum[i][clusterId] / nPoints[clusterId];
				if (previous_coor[i] != c->coor[i])
					changed = true;
			}
		}
		
		#ifdef DEBUG
		cout << "Finished iteration number " << iter << "." << endl;
		#endif
		
		if (changed == false)    //Check if kmeans algorithm has converged
		{
			cout << "Kmeans algorithm has converged. Total number of iterations: " << iter << endl;
			break;  
		}
		if (iter == epochs - 1)
		{
			cout << "Kmeans algorithm has reached maximum number of iterations, but not converged. Total number of iterations: " << iter + 1 << endl;
			changed = false; //ending while loop
			break;
		}
	}
}

void assignLabelToCluster(vector<Image>* points, int k, int *label_clust)
{
	int *label_num = new int[k];   //contains the number of label occurence in one cluster
	
	int pointsSize = points->size();
	vector<Image>::iterator it_begin = points->begin();
	vector<Image>::iterator it;
	
	for (int i = 0; i < k; i++)
	{
		//Initialization of label numbers
		for (int j = 0; j < k; j++)
		{
			label_num[j] = 0;
		}
		
		//Counting labels for cluster i	
		for (int j = 0; j < pointsSize; j++) 
		{
			it = it_begin + j;
			int clusterId = it->cluster;
			int original_label = it->label;
			
			if (clusterId == i)
				label_num[original_label] += 1; 
		}
		
		//The most numerous label becomes a cluster reprezentation number from range (1,10)
		int max = label_num[0];
		label_clust[i] = 0;
		for (int j = 0; j < k; j++)
		{
			if (label_num[j] > max)
			{
				max = label_num[j];
				label_clust[i] = j;
			}
		}
	}
	delete [] label_num;
}

/*
void assignLabelToCluster(vector<Image>* points, int k, int *label_clust)
{
//	int *label_num = new int[k];   //contains the number of label occurence in one cluster
	int *label_num = new int[k*k];   //[k][k], each label_num[i] contains the number of label occurence in one cluster (i. cluster)
	
	int pointsSize = points->size();
	vector<Image>::iterator it_begin = points->begin();
	vector<Image>::iterator it;
	
	#pragma omp parallel for num_threads(tc) schedule(OMP_SCHEDULE,1)
	for (int i = 0; i < k; i++)
	{
		//Initialization of label numbers
		for (int j = 0; j < k; j++)
		{
			label_num[i*k + j] = 0;
		}
		
		//Counting labels for cluster i	
		for (int j = 0; j < pointsSize; j++) 
		{
			it = it_begin + j;
			int clusterId = it->cluster;
			int original_label = it->label;
			
			if (clusterId == i)
				label_num[i*k + original_label] += 1; 
		}
		
		//The most numerous label becomes a cluster reprezentation number from range (1,10)
		int max = label_num[i*k];
		label_clust[i] = 0;
		for (int j = 0; j < k; j++)
		{
			if (label_num[i*k + j] > max)
			{
				max = label_num[i*k + j];
				label_clust[i] = j;
			}
		}
	}
	delete [] label_num;
}
*/

int predict(Image img, vector<Image> centroids, int *label_clust)
{
	int lab;
	double dist;
	int centroidsSize = centroids.size();
	vector<Image>::iterator c_begin = begin(centroids);
	vector<Image>::iterator c;
	
	for (int j = 0; j < centroidsSize; j++) 
	{
		c = c_begin + j;
		lab = label_clust[j];
		dist = c->distance(img);
		if (dist < img.minDist)
		{
			img.minDist = dist;
			img.label = lab;
		}
	}
	return img.label; 
}

double calculateTrainingAccuracy(vector<Image>* points, int *label_clust)
{
	double total_labels = (double)n; //all points have label
	double correct_labels = 0;  //points with correct classification
	int original_label;
	
	int pointsSize = points->size();
	vector<Image>::iterator it_begin = points->begin();
	vector<Image>::iterator it;
	
	#pragma omp parallel for num_threads(tc) schedule(static,1) reduction(+: correct_labels)
	for (int i = 0; i < pointsSize; i++) 
	{
		it = it_begin + i;
		original_label = it->label;
		
		if (original_label == label_clust[it->cluster])
		{
			correct_labels++;
		}
	}
	return correct_labels/total_labels;
}

int main(int argc, char *argv[])
{
	#ifdef PARAM_IN_ARGS
	tc = atoi(argv[1]);
	char *csv_train = argv[2];
	char *csv_test = argv[3]; 
	int k = atoi(argv[4]);
	int ep = atoi(argv[5]);
	ch_size = atoi(argv[6]);
	num_for_training = atoi(argv[7]);
	#else
	cout << "Number of threads:" << endl;
	cin >> tc;
	
	cout << "Insert relative path to CSV file of TRAINING dataset:" << endl;
	char csv_train[30];
	cin >> csv_train;
	
	cout << "Insert relative path to CSV file of TEST dataset:" << endl;
	char csv_test[30];
	cin >> csv_test;
	
	int k;		//number of clusters
	cout << "Insert k: " << endl;
	cin >> k;		
			  
	int ep;	//number of iterations  
	cout << "Insert maximum number of iterations:" << endl;
	cin >> ep; 
	
	cout << "Insert chunksize parameter for omp schedule:" << endl;
	cin >> ch_size;
	
	cout << "Insert number of images for training" << endl;
	cin >> num_for_training;
	#endif
	
	cout << "TRAINING STARTED." << endl;
	
	//Start measuring time
	double t;
   	t = omp_get_wtime();
   	
	//Training
	vector<Image> points = readCsv(csv_train); 
	cout << "Number of images: " << n << endl;
	//cout << "Training dataset has been read" << endl;
	
	//Execute algorithm, training
	vector<Image> centroids;
	kMeansClustering(&points, ep, k, &centroids); 
	
	//Calculate which cluster is which number
	int *label_clust = new int[k]; //meaning of cluster (number)
	assignLabelToCluster(&points, k, label_clust);
	
	//Finish measuring time
	double time_taken = omp_get_wtime() - t; //in seconds
	
	cout << "Training finished." << endl << endl;
	cout << "Time spent: " << time_taken <<" s." << endl << endl;
	
	//Calculate accuracy between true labels and kmeans labels
	double accuracy = calculateTrainingAccuracy(&points, label_clust);
	
	cout << "* Accuracy for training set is " << 100*accuracy << "%." << endl; 
	
	//Testing
	n = 0;
	vector<Image> test_points = readCsv(csv_test);
	
	int testSize = test_points.size();
	vector<Image>::iterator t_begin = test_points.begin();
	vector<Image>::iterator tp;
	double correct_labels = 0;
	#pragma omp parallel for num_threads(tc) schedule(OMP_SCHEDULE,ch_size) private(tp) reduction(+: correct_labels)
	for (int i = 0; i < testSize; i++)
	{
		tp = t_begin + i;
		int predicted_number = predict(*tp, centroids, label_clust);
		if (predicted_number == tp->label) 
			correct_labels++;
	}
	
	accuracy = correct_labels/(double)n; //n is now number of points in test set

	cout << "* Accuracy for test set is " << 100*accuracy << "%." << endl; 
	
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
	
	/* Uncomment for different type of report 
	for (vector<Image>::iterator it = begin(test_points); it != end(test_points); ++it) 
	{
		myfile << it->label << "," << label_clust[it->cluster] << endl;
	}
	*/
		
	myfile.close();
	
	cout << "Cluster centroids are stored in ouput csv file." << endl << endl;
	
	delete [] label_clust;
	
	return 0;
}

