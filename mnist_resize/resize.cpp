#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

int main() {
    // Load MNIST dataset from CSV file
    ifstream file("mnist_train.csv");
    string line;
    vector<vector<int>> X_flat;
    vector<int> y;

    // Read and discard the header line
    getline(file, line);

    while (getline(file, line)) {
        istringstream ss(line);
        string token;
        getline(ss, token, ',');
        int label;
        try {
            label = stoi(token);
        } catch (const invalid_argument& e) {
            cerr << "Invalid label: " << e.what() << endl;
            continue; // Skip this line
        }
        y.push_back(label);

        vector<int> pixels;
        while (getline(ss, token, ',')) {
            try {
                pixels.push_back(stoi(token));
            } catch (const invalid_argument& e) {
                cerr << "Invalid pixel value: " << e.what() << endl;
                pixels.push_back(0); // Set to default value (0)
            }
        }
        X_flat.push_back(pixels);
    }

    // Resize images from 28x28 to 14x14
    vector<vector<int>> X_resized;
    for (const auto& pixels : X_flat) {
        Mat image(28, 28, CV_8UC1);
        for (int i = 0; i < 28; ++i) {
            for (int j = 0; j < 28; ++j) {
                image.at<uchar>(i, j) = pixels[i * 28 + j];
            }
        }
        Mat resized_image;
        resize(image, resized_image, Size(14, 14));
        vector<int> resized_pixels;
        for (int i = 0; i < 14; ++i) {
            for (int j = 0; j < 14; ++j) {
                resized_pixels.push_back(resized_image.at<uchar>(i, j));
            }
        }
        X_resized.push_back(resized_pixels);
    }

    // Save resized dataset to CSV file
    ofstream out_file("mnist_train_resized.csv");
    out_file << "label";
    for (int i = 0; i < 196; ++i) {
        out_file << ",pixel_" << i;
    }
    out_file << "\n";

    for (int i = 0; i < X_resized.size(); ++i) {
        out_file << y[i];
        for (const auto& pixel : X_resized[i]) {
            out_file << "," << pixel;
        }
        out_file << "\n";
    }
    out_file.close();

    // Save the resized images as JPEG files
    for (int i = 0; i < 10 && i < X_resized.size(); ++i) {
        Mat image(14, 14, CV_8UC1);
        for (int row = 0; row < 14; ++row) {
            for (int col = 0; col < 14; ++col) {
                image.at<uchar>(row, col) = X_resized[i][row * 14 + col];
            }
        }
        stringstream ss;
        ss << "image_" << i + 1 << "_label_" << y[i] << ".jpg";
        imwrite(ss.str(), image);
    }

    return 0;
}
