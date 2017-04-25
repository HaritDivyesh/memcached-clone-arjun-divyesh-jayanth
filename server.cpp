#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>

/*
* Max pending connections queue length
*/
#define MAX_CONNECTIONS 25
#define CLIENT_BUFFER_SIZE 1024
#define MEMCACHED_PORT 11211

void *handle_client(int client_sockfd)
{
	char buffer[CLIENT_BUFFER_SIZE] = {0};
	printf("%s\n", "New client!!");

	/*
	* read each command from the client
	*/
	while(1) {
		memset(buffer, 0, sizeof buffer);
		read(client_sockfd, buffer, sizeof buffer);

		if(strncmp(buffer, "get", 3) == 0) {
			printf("%s: %s\n", "Received command", buffer);
			continue;
		}

		if(strncmp(buffer, "put", 3) == 0) {
			printf("%s: %s\n", "Received command", buffer);
			continue;
		}
	}
	/* to suppress compiler warning */
	return NULL;
}

int main(void)
{
	int sockfd, client_sockfd;
	struct sockaddr_in server_addr, client_addr;
	static unsigned short port = MEMCACHED_PORT;
	unsigned int addrlen = sizeof(client_addr);

	/*
	* TODO: initiate mutexes for client data structures here.
	*/

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
