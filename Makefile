EXECUTABLE      := myrtt.c mympi.a
OUTPUT := myrtt

CXX        := g++
CC         := gcc
MPICC         := mpicc
LINK       := g++ -fPIC

INCLUDES  += -I. -I/ncsu/gcc346/include/c++/ -I/ncsu/gcc346/include/c++/3.4.6/backward 
LIB       := -L/ncsu/gcc346/lib

default:
	$(CC) -g -c mympi.c -o mympi.o
	ar ruv mympi.a mympi.o
	ranlib mympi.a
	$(CC) -o $(OUTPUT) -lpthread -lm $(EXECUTABLE)

clean:
	rm -f $(OUTPUT)


