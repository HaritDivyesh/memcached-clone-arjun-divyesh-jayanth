#include "memcached.hh"
#include "cache.cpp"
#include "expiry_collector.cpp"
#include "handle.cpp"

/* factor for kilo, mega, giga */
static size_t get_multiplier(char c) {
	if (c == 'k' || c == 'K')
		return 1024;
	if (c == 'm' || c == 'M')
		return 1024 * 1024;
	if (c == 'g' || c == 'G')
		return 1024 * 1024 * 1024;
	return 1;
}

int main(int argc, char **argv)
{
	int sockfd, client_sockfd;
	struct sockaddr_in server_addr, client_addr;
	static unsigned short port = MEMCACHED_PORT;
	unsigned int addrlen = sizeof(client_addr);
	srand(time(NULL));

	if (argc < 2) {
		fprintf(stderr, "%s\n", "usage: memo <MEMORY-THRESHOLD> [--replacement=lru|random|landlord]");
		exit(1);
	}

	if (!atoi(argv[1])) {
		fprintf(stderr, "%s\n", "usage: memo <MEMORY-THRESHOLD> [--replacement=lru|random|landlord]");
		exit(1);
	}

	memory_limit = atoi(argv[1]) * get_multiplier(argv[1][strlen(argv[1]) - 1]);
	if (argc > 2) {
		/* already initialized as lru, so don't bother if --replacement=lru is provided */
		if (strncmp(argv[2], "--replacement=random", 20) == 0) {
			policy = RANDOM;
		}
		else if (strncmp(argv[2], "--replacement=landlord", 22) == 0) {
			policy = LANDLORD;
		}
	}

	printf("Memo beginning {memory limit: %zu, replacement algo: %s}\n",
		memory_limit, policy == 2 ? "Landlord" : (policy == 1 ? "Random" : "LRU"));
	init_replacement();
	
	std::thread collection = std::thread(trigger_collection);
	collection.detach();

	/*
	* create a TCP socket
	*/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(sockfd, (struct sockaddr*) &server_addr, sizeof server_addr);
	listen(sockfd, MAX_CONNECTIONS);


	/*
	* Handle each client in a new thread via handle_client().
	* Main thread waits for subsequent clients.
	*/
	while(1) {
		client_sockfd = accept(sockfd, (struct sockaddr*) &client_addr, &addrlen);
		process_stats->curr_connections++;
		process_stats->total_connections++;
		std::thread t = std::thread(handle_client, client_sockfd);
		t.detach();
	}

	return 0;
}
