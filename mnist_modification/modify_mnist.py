import csv
import os
import sys

# Path to the MNIST CSV file
mnist_csv_path = 'mnist/mnist_train.csv'

# Create a directory to store the individual image files
output_directory = 'mnist_cluster_distance'
out_filename = 'mnist_train_c_d.csv'

# Ensure the output directory exists
os.makedirs(output_directory, exist_ok=True)

# Open output file
csv_path = os.path.join(output_directory, out_filename)

# Read the MNIST CSV file and write to the new CSV file
with open(mnist_csv_path, 'r') as csvfile, open(csv_path, 'w', newline='') as csv_file:
    csvreader = csv.reader(csvfile)
    csv_writer = csv.writer(csv_file)

    # Process each row in the CSV file
    for i, row in enumerate(csvreader):
        # Add two additional columns (cluster and minDistance)
        row.extend(['-1', str(sys.float_info.max)])

        # Write the modified row to the new CSV file
        csv_writer.writerow(row)

print(f"Modification complete. New MNIST CSV file is saved in the '{output_directory}' directory.")