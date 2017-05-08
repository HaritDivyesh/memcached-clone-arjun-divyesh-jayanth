CC = g++

tests:
	$(CC) -std=c++11 -Wall -g -pthread test/tester.cpp -o test/tester

runtests: all tests
	./test/tester test/input/basic.txt test/expected_output/basic.txt

all:
	$(CC) -std=c++11 -Wall -g -pthread memcached.cpp -o memcached
