#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>

#define MEMCACHED_PORT 11211
#define BUFFER_SIZE 1024

/*
void get(int sockfd, const char *key)
{
	char buff[BUFFER_SIZE] = {0};
	strcat(buff, "get ");
	strcat(buff, key);
	printf("%s: %s\n", "Issuing command", buff);
	write(sockfd, buff, sizeof buff);
}

void set(int sockfd, const char *key, const char *val)
{
	char buff[BUFFER_SIZE] = {0};
	strcat(buff, "set ");
	strcat(buff, key);
	strcat(buff, " ");
	strcat(buff, val);
	printf("%s: %s\n", "Issuing command", buff);
	write(sockfd, buff, sizeof buff);
}
*/

void console(int sockfd)
{
	char buff[BUFFER_SIZE] = {0};

	while (1) {
		fgets(buff, sizeof buff, stdin);
		buff[strlen(buff) - 1] = '\0';
		write(sockfd, buff, sizeof buff);
	}
}

int main(void)
{
	int sockfd;
	struct sockaddr_in server_addr;
	static unsigned short port = MEMCACHED_PORT;

	/*
	* create a TCP socket
	*/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Connect to the memcached server */
	connect(sockfd, (struct sockaddr*) &server_addr, sizeof server_addr);

	console(sockfd);

	return 0;
}
