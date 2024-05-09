import csv
import os
import struct

num_per_file = 250

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
            
        # Convert pixel values to bytes
        pixel_bytes = bytes([int(pixel) for pixel in row])
        
        # Add two additional bytes (-1 for cluster and 32-bit max integer for minDist)
        additional_bytes = b'\xFF' + b'\x7F' + b'\xFF' * 3
        
        # Save the pixel bytes to a binary file
        binary_filename = f'img_{file_iterator}.bin'
        binary_path = os.path.join(output_directory, binary_filename)
        
        # Write the pixel bytes to the new binary file
        with open(binary_path, 'ab') as binary_file:
            binary_file.write(pixel_bytes)
            binary_file.write(additional_bytes)

print(f"Separation complete. Binary files are saved in the '{output_directory}' directory.")