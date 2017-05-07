#ifndef MEMCACHED_HH
#define MEMCACHED_HH

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cctype>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <string>
#include <map>
#include <sstream>
#include <mutex>
#include <float.h>
#include <chrono>

#define VERSION_STR "0.0.1"

/* Max pending connections queue length*/
#define MAX_CONNECTIONS 25
#define CLIENT_BUFFER_SIZE 1024
#define MEMCACHED_PORT 11211
/* 1 MB */
/* #define MEMORY_THRESHOLD 1048576 */
#define MEMORY_THRESHOLD 250
#define WHITESPACE " \t\n\v\f\r"

#define _WRITER(X) write(client_sockfd, X "\r\n", sizeof(X "\r\n"))

/*
* Replies
*/
#define STORED _WRITER("STORED")
#define NOT_STORED _WRITER("NOT_STORED")
#define EXISTS _WRITER("EXISTS")
#define NOT_FOUND _WRITER("NOT_FOUND")
#define DELETED _WRITER("DELETED")
#define TOUCHED _WRITER("TOUCHED")
#define END _WRITER("END")
#define VERSION _WRITER("VERSION " VERSION_STR)

/*
* Error strings
*/
#define ERROR _WRITER("ERROR")
#define CLIENT_ERROR(X) _WRITER("CLIENT_ERROR " X)
#define SERVER_ERROR(X) _WRITER("SERVER_ERROR " X)


/*
* This is the Cache Entry data structure stored as value in the map
*/
typedef struct {
	std::string key;
	uint16_t flags;
	int64_t expiry;
	/* not including the delimiting \r\n */
	size_t bytes;
	uint64_t cas_unique;
	bool noreply;
	char *data;
} cache_entry;


/*
* MCMap is the map data structure that stores the hash-table
*/
typedef std::map<std::string, cache_entry> MCMap;
static MCMap *map = new MCMap();
static std::mutex map_mutex;
static unsigned memory_counter = 0;

/*
* Type for replacement policies
*/

typedef enum {
	LRU,
	RANDOM,
	LANDLORD
} policy_t;

typedef struct node_t node_t;
struct node_t {
	cache_entry* entry;
	node_t *prev;
	node_t *next;
        float cost;   // for landlord credit
};
static node_t *head = NULL;
static node_t *tail = NULL;
static size_t list_size = 0;
static float delta = FLT_MAX; // for current min(credit(entry)/size(entry))

/* default value is LRU */
static policy_t policy = LRU;

/*void init_replacement(void);
policy_t get_replacement_policy(void);
int run_replacement(size_t);
void add_to_list(cache_entry* entry);*/

static void write_VALUE(int client_sockfd, cache_entry *entry, unsigned gets_flag)
{
	std::ostringstream os;  
	os << "VALUE " << entry->key << " " << entry->flags << " " << entry->bytes;
	if (gets_flag)
		os << " " << entry->cas_unique;
	os << "\r\n";
	write(client_sockfd, os.str().c_str(), strlen(os.str().c_str()));
}

static void print_map(MCMap *map)
{
	for (const auto &p : *map) {
		std::cout << "map[" << p.first << "] = ";
		std::cout << p.second.key << ", ";
		std::cout << p.second.flags << ", ";
		std::cout << p.second.expiry << ", ";
		std::cout << p.second.bytes << '\n';
	}
}

#endif
