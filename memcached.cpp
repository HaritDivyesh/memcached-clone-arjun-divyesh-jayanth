#include "memcached.hh"
#include "cache.cpp"
#include "handle.cpp"

int main(int argc, char **argv)
{
	int sockfd, client_sockfd;
	struct sockaddr_in server_addr, client_addr;
	static unsigned short port = MEMCACHED_PORT;
	unsigned int addrlen = sizeof(client_addr);
	srand(time(NULL));

	if (argc > 1) {
		/* already initialized as lru, so don't bother if -lru is provided */
		if (strncmp(argv[1], "-random", 7) == 0) {
			policy = RANDOM;
		}
		else if (strncmp(argv[1], "-landlord", 9) == 0) {
			policy = LANDLORD;
		}
	}
	init_replacement();

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
		std::thread t = std::thread(handle_client, client_sockfd);
		t.detach();
	}

	return 0;
}
