import os


# Path to the directory containing binary files
binary_directory = 'mnist_train_images'

# Name of the file to read
file_name = 'img_0.bin'

# Construct the full path to the file
binary_path = os.path.join(binary_directory, file_name)

# Check if the file exists
if os.path.isfile(binary_path):
    # Read the contents of the binary file
    with open(binary_path, 'rb') as file:
        binary_data = file.read()
        print(f"Contents of {file_name}: {binary_data}")
else:
    print(f"File {file_name} not found in directory {binary_directory}.")