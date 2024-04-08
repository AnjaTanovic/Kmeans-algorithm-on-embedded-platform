import csv
import os
import sys

num_per_file = 100

# Path to the MNIST CSV file
mnist_csv_path = 'mnist_resize/mnist_train_resized.csv'  

# Create a directory to store the individual image files
output_directory = 'mnist_train_images'
os.makedirs(output_directory, exist_ok=True)

file_iterator = 0
max_int_32bit = 2**31 - 1  # Maximum 32-bit integer value

# Read the MNIST CSV file
with open(mnist_csv_path, 'r') as csvfile:
    csvreader = csv.reader(csvfile)
    
    # Skip the header if it exists
    header = next(csvreader, None)
    
    # Process each row in the CSV file
    for i, row in enumerate(csvreader):
    
        if (i % num_per_file == 0) and (i != 0):
            file_iterator = file_iterator + 1
            
        # Add two additional columns (cluster and minDistance)
        row.extend(['-1', str(max_int_32bit)])
        
        # Save the row as a separate CSV file
        csv_filename = f'img_{file_iterator}.csv'
        csv_path = os.path.join(output_directory, csv_filename)
        
        # Write the row to the new CSV file
        with open(csv_path, 'a', newline='') as csv_file:
            csv_writer = csv.writer(csv_file)
            csv_writer.writerow(row)

print(f"Separation complete. CSV files are saved in the '{output_directory}' directory.")