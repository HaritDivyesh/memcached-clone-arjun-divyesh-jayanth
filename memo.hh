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
#define WHITESPACE " \t\n\v\f\r"

#define _WRITER(X){\
 write(client_sockfd, X "\r\n", sizeof(X "\r\n"));\
 process_stats->bytes_written += sizeof(X "\r\n");\
 }
#define STAT_WRITER(X) write(client_sockfd, X, sizeof(X))

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
#define OK _WRITER("OK")
#define VERSION _WRITER("VERSION " VERSION_STR)
#define STAT(x) STAT_WRITER(x)
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
static size_t memory_limit;

static std::map<std::string, long int> *cache_miss_map = new std::map<std::string, long int>();

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
static std::mutex list_mutex;

/* default value is LRU */
static policy_t policy = LRU;


typedef struct{
  int32_t pid = getpid();
  int32_t start_time = std::time(NULL); //for calculation of uptime
  int32_t uptime;
  int32_t time;
  const char *version = VERSION_STR;
  int32_t pointer_size = sizeof(uintptr_t)*8;
  int32_t rusage_user;
  int32_t rusage_system;
  int32_t curr_items = 0;
  int32_t total_items = 0;
  int64_t bytes = 0;
  int32_t curr_connections = 0;
  int32_t total_connections = 0;
  int32_t connection_structures = 0;
  int32_t reserved_fds = 0;
  int64_t cmd_get = 0;
  int64_t cmd_set = 0;
  int64_t cmd_flush = 0;
  int64_t get_hits = 0;
  int64_t get_misses = 0;
  int64_t delete_misses = 0;
  int64_t delete_hits = 0;
  int64_t incr_misses = 0;
  int64_t incr_hits = 0;
  int64_t decr_misses = 0;
  int64_t decr_hits = 0;
  int64_t cas_misses = 0; 
  int64_t cas_hits = 0;
  int64_t cas_badval = 0;
  int64_t evictions = 0;
  int64_t reclaimed = 0;
  int64_t bytes_read = 0;
  int64_t bytes_written = 0;
  int32_t limit_maxbytes = memory_limit;
  int32_t threads = 0; 
} stats;

stats *process_stats = new stats();

static void write_VALUE(int client_sockfd, cache_entry *entry, unsigned gets_flag)
{
	std::ostringstream os;  
	os << "VALUE " << entry->key << " " << entry->flags << " " << entry->bytes;
	if (gets_flag)
		os << " " << entry->cas_unique;
	os << "\r\n";
	write(client_sockfd, os.str().c_str(), strlen(os.str().c_str()));
}

/*static void print_map(MCMap *map)
{
	for (const auto &p : *map) {
		std::cout << "map[" << p.first << "] = ";
		std::cout << p.second.key << ", ";
		std::cout << p.second.flags << ", ";
		std::cout << p.second.expiry << ", ";
		std::cout << p.second.bytes << '\n';
	}
}*/


#endif
