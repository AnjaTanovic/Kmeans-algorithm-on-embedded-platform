import matplotlib.pyplot as plt
import numpy as np
import csv
from sklearn.decomposition import PCA

# Function to read data from CSV
def read_csv(filename):
    data = []
    labels = []
    with open(filename, 'r') as file:
        reader = csv.reader(file)
        header = next(reader)  # skip header
        for row in reader:
            labels.append(int(row[0]))
            data.append([int(value) for value in row[1:]])
    return np.array(data), np.array(labels)

# Filename of the CSV file
filename = 'testing/mnist_train.csv'

# Read the dataset from the CSV file
X_train, y_train_true = read_csv(filename)

# Perform PCA for dimensionality reduction
pca = PCA(n_components=2)
X_train_pca = pca.fit_transform(X_train)

# Normalize PCA output to range 0 to 255 (not an important step, this is just for visualization)
X_train_pca_min = X_train_pca.min(axis=0)
X_train_pca_max = X_train_pca.max(axis=0)
X_train_pca = ((X_train_pca - X_train_pca_min) / (X_train_pca_max - X_train_pca_min) * 255).astype(int)

# Plot the reduced data in 2D
plt.scatter(X_train_pca[:, 0], X_train_pca[:, 1], c=y_train_true, cmap='viridis', s=10)
plt.xlabel('Principal Component 1')
plt.ylabel('Principal Component 2')
plt.title('PCA Visualization of High-dimensional Data (2D)')
plt.colorbar(label='Cluster')
plt.show()