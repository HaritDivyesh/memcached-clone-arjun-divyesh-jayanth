CC = g++

all:
	$(CC) -std=c++11 -Wall -g -lpthread memcached.cpp -o memcached
	$(CC) -std=c++11 -Wall -g -lpthread naive_client.cpp -o naive_client
