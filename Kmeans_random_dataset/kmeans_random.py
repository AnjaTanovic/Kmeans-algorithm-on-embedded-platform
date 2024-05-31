import matplotlib.pyplot as plt
import numpy as np
import csv
from sklearn.datasets import make_blobs
from sklearn.decomposition import PCA

dimensionality = 50     #number of dimensions of one point (for example, mnist has 784)
cluster_spread = 0.80   #how much the clusters spill over and overlap
num_of_data = 1000      #number of points in whole dataset
num_centers = 20        #k for kmeans

X_train, y_train_true = make_blobs(n_samples=num_of_data, centers=num_centers,
                                   cluster_std=cluster_spread, random_state=0, n_features = dimensionality)

# Normalize data to range 0 to 255 and convert to integers
X_train_min = X_train.min(axis=0)
X_train_max = X_train.max(axis=0)
X_train = ((X_train - X_train_min) / (X_train_max - X_train_min) * 255).astype(int)

# Save dataset to CSV
header = ['label'] + [f'feature_{i+1}' for i in range(dimensionality)]
with open('dataset.csv', 'w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(header)
    for features, label in zip(X_train, y_train_true):
        writer.writerow([label] + features.tolist())

# Plot first 2 dimensions of data
#plt.scatter(X_train[:, 0], X_train[:, 1], s=10)
#plt.show()

# VISUALIZATION
# Perform PCA for dimensionality reduction
pca = PCA(n_components=2)
X_train_pca = pca.fit_transform(X_train)

# Normalize PCA output to range 0 to 255 (not important step, this is just for visualization)
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