LIB= -lboost_system  -lboost_filesystem -lopencv_calib3d -lopencv_features2d -lopencv_imgproc -lopencv_highgui -lopencv_core
INC= -I../
STD= -std=c++11
CC = g++ $(STD)

install:all clean
clean:
	rm *.o
all: donkey.o qbic.o
	$(CC) test/test.cpp donkey.o qbic.o  $(LIB) $(INC) -o test/feature

qbic.o: qbic.cpp config.h
	$(CC) -c qbic.cpp

donkey.o:
	$(CC) -c ../../src/donkey.cpp -o donkey.o
