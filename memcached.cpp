#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <string>
#include <map>
#include <sstream>
#include <mutex>

#include "memcached.hh"


static void handle_client(int client_sockfd, std::map<std::string, cache_entry> *map)
{
	printf("Client %d connected.\n", client_sockfd);
	char buffer[CLIENT_BUFFER_SIZE] = {0};

	/*
	* read each command from the client
	*/
	while(1) {
		memset(buffer, 0, sizeof buffer);
		read(client_sockfd, buffer, sizeof buffer);

		if(strncmp(buffer, "quit", 4) == 0) {
			close(client_sockfd);
			return;
		}

		if(strncmp(buffer, "get ", 4) == 0) {
			/* print_map(map); */
			buffer[strcspn(buffer, "\r\n")] = '\0';
			char *key = strtok(buffer + 4, WHITESPACE);
			while (key) {
				if ((*map).count(key) != 0) {
					cache_entry *entry = &(*map)[key];
					write_VALUE(client_sockfd, entry);
					write(client_sockfd, entry->data, entry->bytes + 2);
				}
				key = strtok(NULL, WHITESPACE);
			}
			END;
			continue;
		}

		if(strncmp(buffer, "set ", 4) == 0) {
			cache_entry *entry = (cache_entry*) malloc(sizeof(cache_entry));
			entry->key = strtok(buffer + 4, WHITESPACE);
			entry->flags = atoi(strtok(NULL, WHITESPACE));
			entry->expiry = atoi(strtok(NULL, WHITESPACE));
			entry->bytes = atoi(strtok(NULL, WHITESPACE));

			/* Read actual data and add to map*/
			memset(buffer, 0, sizeof buffer);
			ssize_t len = read(client_sockfd, buffer, sizeof buffer);
			/* 2 is the size of \r\n */
			len -= 2;
			if (len < 1) {
				free(entry);
				ERROR;
				CLIENT_ERROR("bad data chunk");
				continue;
			}
			if (len > entry->bytes) {
				free(entry);
				ERROR;
				continue;
			}
			/* reassign so that bytes is not greater than len */
			entry->bytes = (uint32_t)len;
			entry->data = (char*)malloc(entry->bytes + 2);
			memcpy(entry->data, buffer, entry->bytes + 2);
			(*map)[entry->key] = *entry;
			STORED;
			continue;
		}
	}
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

	std::map<std::string, cache_entry> *map = new std::map<std::string, cache_entry>();

	/*
	* Handle each client in a new thread via handle_client().
	* Main thread waits for subsequent clients.
	*/
	while(1) {
		client_sockfd = accept(sockfd, (struct sockaddr*) &client_addr, &addrlen);
		std::thread t = std::thread(handle_client, client_sockfd, map);
		t.detach();
	}

	return 0;
}
