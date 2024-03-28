bash shell
#!/bin/bash

#k = 10, 100, 200
# Array of arguments
arguments=(
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 60000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 50000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 40000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 30000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 20000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 10000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 8000"  
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 5000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 3000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 1000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 800"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 500"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 300"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 20 100"

"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 60000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 50000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 40000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 30000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 20000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 10000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 8000"  
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 5000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 3000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 1000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 800"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 500"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 300"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 50 200 20 100"

"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 60000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 50000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 40000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 30000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 20000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 10000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 8000"  
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 5000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 3000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 1000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 800"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 500"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 300"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 100 200 20 100"

"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 60000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 50000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 40000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 30000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 20000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 10000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 8000"  
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 5000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 3000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 1000"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 800"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 500"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 300"
"6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 200 200 20 100")

# Loop through each argument and execute the program
for arg in "${arguments[@]}"
do
    echo "Running program with arguments: $arg"
    ./kmeans $arg
    wait
done
