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
#include <mutex>

/*
* Max pending connections queue length
*/
#define MAX_CONNECTIONS 25
#define CLIENT_BUFFER_SIZE 1024
#define MEMCACHED_PORT 11211


typedef struct {
	std::string key;
	uint16_t flags;
	int64_t expiry;
	/* not including the delimiting \r\n */
	uint32_t bytes;
	uint64_t cas_unique;
	bool noreply;
	char *data;
} cache_entry;


void print_map(std::map<std::string, cache_entry> *map)
{
	for (const auto &p : *map) {
		std::cout << "map[" << p.first << "] = ";
		std::cout << p.second.key << ", ";
		std::cout << p.second.flags << ", ";
		std::cout << p.second.expiry << ", ";
		std::cout << p.second.bytes << '\n';
	}
}

void *handle_client(int client_sockfd, std::map<std::string, cache_entry> *map)
{
	char buffer[CLIENT_BUFFER_SIZE] = {0};
	printf("%s\n", "New client!!");

	/*
	* read each command from the client
	*/
	while(1) {
		memset(buffer, 0, sizeof buffer);
		read(client_sockfd, buffer, sizeof buffer);

		if(strncmp(buffer, "get ", 4) == 0) {
			/*printf("%s: %s\n", "Received command", buffer);*/
			buffer[strcspn(buffer, "\r\n")] = '\0';
			char *key = buffer + 4;
			/*print_map(map);*/
			if ((*map).count(key) != 0) {
				cache_entry *entry = &(*map)[key];
				write(client_sockfd, entry->data, entry->bytes);
			}
			write(client_sockfd, "END\r\n", sizeof("END\r\n"));
			continue;
		}

		if(strncmp(buffer, "set ", 4) == 0) {
			/*printf("%s: %s\n", "Received command", buffer);*/
			cache_entry *entry = (cache_entry*) malloc(sizeof(cache_entry));
			entry->key = strtok(buffer + 4, " ");
			entry->flags = atoi(strtok(NULL, " "));
			entry->expiry = atoi(strtok(NULL, " "));
			entry->bytes = atoi(strtok(NULL, " "));

			/* Read actual data and add to map*/
			memset(buffer, 0, sizeof buffer);
			ssize_t len = read(client_sockfd, buffer, sizeof buffer);
			if (len < 1) {
				free(entry);
				write(client_sockfd, "ERROR\r\n", sizeof("ERROR\r\n"));
				continue;
			}
			/* 2 is the size of \r\n */
			if (len - 2 > entry->bytes) {
				free(entry);
				write(client_sockfd,
					"CLIENT_ERROR bad data chunk\r\n",
					sizeof("CLIENT_ERROR bad data chunk\r\n"));
				write(client_sockfd, "ERROR\r\n", sizeof("ERROR\r\n"));
				continue;
			}
			/* reassign so that bytes is not greater than len */
			entry->bytes = (uint32_t)len;
			entry->data = (char*)malloc(entry->bytes);
			memcpy(entry->data, buffer, entry->bytes);
			(*map)[entry->key] = *entry;
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
