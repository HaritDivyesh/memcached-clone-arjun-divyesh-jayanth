# Memo (memcached close)


Authors: Arjun Sreedharan, Divyesh Harit, Jayanth Hegde

Status: Done


### Requirements

* g++


## Run

To build the entire code, do:

```
make all
```

Optionally, to build just the development code, do:

```
make dev
```

The memo server is pre-configured to run on port **11211**.

Run memo using the following syntax:

````
./memo <MEMORY-THRESHOLD> [--replacement=lru|random|landlord]
````

example:

```
./memo 500 --replacement=lru
````

If no replacement algo is provided, *lru* is run as default.

You may open any client which can connect to port 11211 to interact with memo.

Example:

```
telnet localhost 11211
```


## Tests

To build just the test code, do:

```
make tests
```

**The replacement test code in the test suite assumes that you are running with a memory threshold of 500**. 

In a terminal, start memo server:

```
./memo 500
```

In a new terminal, start the test script:

```
make runtests
````

Sample output of tests can be found in `test/test_output.txt`.


The tester engine takes as input a script file in the following syntax.

In the script file, for each command the first line will be a positive integer W where W is the number of lines required by the command. This will be followed by W lines of commands
E.g.
   _________________
   | 2             |
   | set meg 1 0 5 |
   | Hello         |
   -----------------
means the command has 2 writes. Any line beginning with a # in the script is a comment. All empty lines are ignored.

## Landlord replacement algorithm

In the landlord algorithm, as a proxy to cost, the difference in time between a *get* miss and succesful *set* for the corresponding key is used. The landlord algorithm has been optmized here my mainitaining the minimal cost element at the head of the linked list. This makes the search for the minimum-cost node a constant time operation.

