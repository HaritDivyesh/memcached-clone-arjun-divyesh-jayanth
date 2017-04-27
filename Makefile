CC = g++

all:
	$(CC) -std=c++11 -Wall -g memcached.cpp -o memcached
	$(CC) -std=c++11 -Wall -g naive_client.cpp -o naive_client
