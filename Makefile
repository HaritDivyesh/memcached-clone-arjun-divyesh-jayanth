CC = g++

all:
	$(CC) -std=c++11 -Wall -g server.cpp -o server
	$(CC) -std=c++11 -Wall -g naive_client.cpp -o naive_client
