CC = "g++"
PROJECT = kmeans
SRC = kmeans.cpp 

$(PROJECT) : $(SRC)
	$(CC) -O2 -fopenmp -Wall $(SRC) -o $(PROJECT)

run:
	./$(PROJECT) 6 ../../../mnist/mnist_train.csv ../../../mnist/mnist_test.csv 10 200 10 10000
	
clean:
	rm kmeans
	
