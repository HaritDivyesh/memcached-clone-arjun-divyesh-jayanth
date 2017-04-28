CC = g++

all:
	$(CC) -std=c++11 -Wall -g -pthread memcached.cpp -o memcached
	$(CC) -std=c++11 -Wall -g -pthread naive_client.cpp -o naive_client
