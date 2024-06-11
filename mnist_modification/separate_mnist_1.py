import csv
import os
import sys

# Path to the MNIST CSV file
mnist_csv_path = 'mnist/mnist_test.csv'  

# Create a directory to store the individual image files
output_directory = 'mnist_test_images'
os.makedirs(output_directory, exist_ok=True)

# Read the MNIST CSV file
with open(mnist_csv_path, 'r') as csvfile:
    csvreader = csv.reader(csvfile)
    
    # Skip the header if it exists
    header = next(csvreader, None)
    
    # Process each row in the CSV file
    for i, row in enumerate(csvreader):
        # Add additional column for cluster
        row.append('-1')
        
        # Save the row as a separate CSV file
        csv_filename = f'img_{i}.csv'
        csv_path = os.path.join(output_directory, csv_filename)
        
        # Write the row to the new CSV file
        with open(csv_path, 'w', newline='') as csv_file:
            csv_writer = csv.writer(csv_file)
            csv_writer.writerow(row)

print(f"Separation complete. CSV files are saved in the '{output_directory}' directory.")