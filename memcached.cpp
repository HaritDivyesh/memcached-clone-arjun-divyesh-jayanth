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


static void handle_client(int client_sockfd)
{
	printf("Client %d connected.\n", client_sockfd);
	char buffer[CLIENT_BUFFER_SIZE] = {0};

	/*
	* read each command from the client
	*/
	while(1) {
		memset(buffer, 0, sizeof buffer);
		read(client_sockfd, buffer, sizeof buffer);

		if (strncmp(buffer, "quit", 4) == 0) {
			close(client_sockfd);
			return;
		}

		if (strncmp(buffer, "get ", 4) == 0) {
			/* print_map(map); */
			buffer[strcspn(buffer, "\r\n")] = '\0';
			char *key = strtok(buffer + 4, WHITESPACE);

			std::lock_guard<std::mutex> guard(map_mutex);
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

		if (strncmp(buffer, "set ", 4) == 0) {
			char *key = strtok(buffer + 4, WHITESPACE);
			if (!key) {
				ERROR;
				continue;
			}

			char *flags = strtok(NULL, WHITESPACE);
			if (!flags) {
				ERROR;
				continue;
			}

			char *expiry = strtok(NULL, WHITESPACE);
			if (!expiry) {
				ERROR;
				continue;
			}

			char *bytes = strtok(NULL, WHITESPACE);
			if (!bytes) {
				ERROR;
				continue;
			}

			cache_entry *entry = (cache_entry*) malloc(sizeof(cache_entry));
			memory_counter += sizeof(cache_entry);
			entry->key = key;
			entry->flags = atoi(flags);
			entry->expiry = atoi(expiry);
			entry->bytes = atoi(bytes);

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

			/* CHECK FOR THRESHOLD BREACH */
			if (memory_counter > MEMORY_THRESHOLD) {
				int ret = run_replacement();
				if (ret) {
					free(entry);
					ERROR;
					SERVER_ERROR("Out of memory");
					continue;
				}
			}


			entry->data = (char*)malloc(entry->bytes + 2);
			memory_counter += entry->bytes + 2;
			memcpy(entry->data, buffer, entry->bytes + 2);

			std::lock_guard<std::mutex> guard(map_mutex);
			(*map)[entry->key] = *entry;
			STORED;
			continue;
		}

		/* default case */
		ERROR;
	}
}

int main(int argc, char **argv)
{
	int sockfd, client_sockfd;
	struct sockaddr_in server_addr, client_addr;
	static unsigned short port = MEMCACHED_PORT;
	unsigned int addrlen = sizeof(client_addr);

	if (argc > 1) {
		/* already initialized as lru, so don't bother if -lru is provided */
		if (strncmp(argv[1], "-random", 7) == 0) {
			policy = RANDOM;
		}
		else if (strncmp(argv[1], "-landlord", 9) == 0) {
			policy = LANDLORD;
		}
	}

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
