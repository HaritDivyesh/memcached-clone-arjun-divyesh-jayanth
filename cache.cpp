/************************** LRU **************************/
static void init_lru(void)
{

}

static void add_to_list_lru(cache_entry* entry)
{
	node_t *node = (node_t*) malloc(sizeof(node_t));
	node->entry = entry;
	node->next = NULL;
	if (!head) {
		node->prev = NULL;
		head = tail = node;
		return;
	}
	node->prev = tail;
	tail->next = node;
	tail = node;
}

static node_t *pop_lru(void)
{
	node_t* tmp;
	if (!tail)
		return NULL;
	tmp = tail;
	tail = tail->prev;
	if (tail)
		tail->next = NULL;
	else
		head = NULL;
	return tmp;
}

static int run_lru(size_t new_item_size)
{
	printf("%s\n", "running LRU.");
	size_t poped_size = 0;
	node_t *poped;
	do {
		printf("%s\n", "popping");
		poped = pop_lru();
		if (!poped)
			return -1;
		printf("%s\n", "popped valid node");
		poped_size += poped->entry->bytes;
		printf("poped size: %lu\n", poped_size);
		free(poped->entry->data);
		memory_counter -= poped_size;
		map->erase(poped->entry->key);
		printf("%s %s\n", "erased key:", poped->entry->key.c_str());
		memory_counter -= sizeof(cache_entry);
		free(poped);
		printf("%s: %u\n", "counter after erase", memory_counter);
	} while (poped_size < new_item_size);
	return 0;
}
/******************** LRU block ends **********************/



/************************* Random *************************/
static void init_random(void)
{
	
}

static void add_to_list_random(cache_entry* entry)
{
	int rand_index;
	node_t *tmp;
	node_t *node = (node_t*) malloc(sizeof(node_t));
	node->entry = entry;
	if (!head) {
		node->prev = node->next = NULL;
		head = tail = node;
		++list_size;
		return;
	}
	rand_index = rand() % list_size;
	tmp = head;
	while (rand_index--) {
		tmp = tmp->next;
	}
	node->prev = tmp;
	node->next = tmp->next;
	if (tmp->next)
		tmp->next->prev = node;
	tmp->next = node;
	if (tmp == tail)
		tail = node;
}

static node_t *pop_random(void)
{
	int rand_index;
	node_t* tmp;
	if (!tail)
		return NULL;
	rand_index = rand() % list_size;
	tmp = head;
	while (rand_index--) {
		tmp = tmp->next;
	}
	if (tmp->prev)
		tmp->prev->next = tmp->next;
	if (tmp->next)
		tmp->next->prev = tmp->prev;
	if (tmp == tail)
		tail = tmp->prev;
	if (tmp == head)
		head = tmp->next;
	return tmp;
}

static int run_random(size_t new_item_size)
{
	printf("%s\n", "running RANDOM.");
	size_t poped_size = 0;
	node_t *poped;
	do {
		printf("%s\n", "popping");
		poped = pop_random();
		if (!poped)
			return -1;
		printf("%s\n", "popped valid node");
		poped_size += poped->entry->bytes;
		printf("poped size: %lu\n", poped_size);
		free(poped->entry->data);
		memory_counter -= poped_size;
		map->erase(poped->entry->key);
		printf("%s %s\n", "erased key:", poped->entry->key.c_str());
		memory_counter -= sizeof(cache_entry);
		free(poped);
		printf("%s: %u\n", "counter after erase", memory_counter);
	} while (poped_size < new_item_size);
	return 0;
}

/******************* Random block ends ********************/


static void init_landlord(void)
{
	
}

static int run_landlord(size_t new_item_size)
{
	return 0;	
}

/*
* Current replacement policy
*/
policy_t get_replacement_policy(void)
{
	return policy;
}

void init_replacement(void)
{
	policy_t curr_policy = get_replacement_policy();
	if (curr_policy == LRU)
		init_lru();
	if (curr_policy == RANDOM)
		init_random();
	if (curr_policy == LANDLORD)
		init_landlord();
}

void add_to_list(cache_entry* entry)
{
	policy_t curr_policy = get_replacement_policy();
	if (curr_policy == LRU)
		add_to_list_lru(entry);
	if (curr_policy == RANDOM)
		add_to_list_random(entry);
	/*if (curr_policy == LANDLORD)
		add_to_list_landlord(entry);*/
}


/* Runs a replacement algo. Success returns 0 */
int run_replacement(size_t new_item_size)
{
	printf("%s\n", "Running cache replacement algo.");
	policy_t curr_policy = get_replacement_policy();
	if (curr_policy == LRU)
		return run_lru(new_item_size);
	if (curr_policy == RANDOM)
		return run_random(new_item_size);
	if (curr_policy == LANDLORD)
		return run_landlord(new_item_size);
	return -1;
}
