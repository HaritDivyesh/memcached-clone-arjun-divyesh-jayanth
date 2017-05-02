#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <string>
#include <map>

#include "cache.hh"

/*
* Current replacement policy
*/
policy_t get_replacement_policy(void)
{
	return policy;
}

/* Runs a replacement algo. Success returns 0 */
int run_replacement(void)
{
	printf("%s\n", "run_replacement()");
	return 0;
}
