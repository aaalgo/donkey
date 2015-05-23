LIB= -lboost_system  -lboost_filesystem -lopencv_core -lopencv_imgproc -lopencv_highgui  
INC= -I../../src/
STD= -std=c++11
CC = g++ $(STD)

install: donkey.o qbic.o
	$(CC) test/test.cpp donkey.o qbic.o  $(LIB) $(INC) -o test/feature
	rm *.o

qbic.o: qbic.cpp config.h
	$(CC) -c qbic.cpp

donkey.o: ../../src/donkey.cpp
	$(CC) -c ../../src/donkey.cpp -o donkey.o
