CC = g++

tests:
	$(CC) -std=c++11 -Wall -g -pthread test/tester.cpp -o test/tester
run_bg:
	./memcached&

runtests: all tests run_bg
	./test/tester
all:
	$(CC) -std=c++11 -Wall -g -pthread memcached.cpp -o memcached
