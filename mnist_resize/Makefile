CC = "g++"
PROJECT = resize
SRC = resize.cpp 

LIBS = `pkg-config opencv4 --cflags --libs`

$(PROJECT) : $(SRC)
	$(CC) -O2 $(SRC) -o $(PROJECT) $(LIBS)

run:
	./$(PROJECT)
	
clean:
	rm resize *.jpg	
