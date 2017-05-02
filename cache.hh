/* #ifndef CACHE_HH
#define CACHE_HH */

/*
* Type for replacement policies
*/

typedef enum {
	LRU,
	RANDOM,
	LANDLORD
} policy_t;

extern policy_t policy;

policy_t get_replacement_policy(void);
int run_replacement(void);

/* #endif */
