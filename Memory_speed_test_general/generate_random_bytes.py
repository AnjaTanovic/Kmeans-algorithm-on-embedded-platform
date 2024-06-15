import os

num_of_bytes = 100000
# Generate random bytes
random_bytes = os.urandom(num_of_bytes)

# Write the random bytes to a binary file
with open('partition/read.bin', 'wb') as file:
    file.write(random_bytes)

print(str(num_of_bytes) + " random bytes written to 'read.bin'")