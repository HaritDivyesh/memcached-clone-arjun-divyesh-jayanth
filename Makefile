CC = g++

tests:
	$(CC) -std=c++11 -Wall -g -pthread test/tester.cpp -o test/tester

runtests: all tests
	./test/tester test/input/basic.txt test/expected_output/basic.txt
	./test/tester test/input/storage.txt test/expected_output/storage.txt
	./test/tester test/input/retrieval.txt test/expected_output/retrieval.txt

all:
	$(CC) -std=c++11 -Wall -g -pthread memcached.cpp -o memcached
