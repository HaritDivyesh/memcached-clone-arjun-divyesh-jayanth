static uint64_t generate_cas_unique(void)
{
	return (int64_t)rand();
}


static void set_expiry(cache_entry *entry){
	if (entry->expiry < 60*60*24*30) {
		if(entry->expiry > 0)
			entry->expiry += std::time(NULL);
	}
}

static int validate_num(char *str)
{
        int  i = 0;
	for(; str[i] != '\0' && isdigit(str[i]); i++);

	if(i == strlen(str))
		return 0;
		
	return 1;
}

static void flush_all(size_t delay)
{
	node_t *curr = head;
	sleep(delay);
	std::lock_guard<std::mutex> guard(map_mutex);
	while (curr) {
		node_t *tmp;
		cache_entry *entry = curr->entry;
		free(entry->data);
		memory_counter -= entry->bytes;
		map->erase(curr->entry->key);
		memory_counter -= sizeof(cache_entry);
		tmp = curr->next;
		free(curr);
		curr = tmp;
	}
	head = tail = NULL;

}

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
		
		process_stats->bytes_read += sizeof(buffer);
		
		if (strncmp(buffer, "quit", 4) == 0) {
			close(client_sockfd);
			process_stats->curr_connections--;
			return;
		}

		if (strncmp(buffer, "version", 4) == 0) {
			VERSION;
			continue;
		}

		/* get and gets */

		if (strncmp(buffer, "get", strlen("get")) == 0) {
			unsigned gets_flag = 0;
			 process_stats->cmd_get++;
			if (strncmp(buffer, "gets ", strlen("gets ")) == 0)
				gets_flag = 1;
			/* Assert not any arbitrary command beginning with get */
			if (strncmp(buffer + strlen("get") + gets_flag, " ", 1) != 0) {
				ERROR;
				continue;
			}

			buffer[strcspn(buffer, "\r\n")] = '\0';
			char *key = strtok(buffer + 4, WHITESPACE);

			std::lock_guard<std::mutex> guard(map_mutex);
			while (key) {
				if ((*map).count(key) == 0) {
					track_misses(key);
					process_stats->get_misses++;
				} else {
					process_stats->get_hits++;
					cache_entry *entry = &(*map)[key];
					write_VALUE(client_sockfd, entry, gets_flag);
					write(client_sockfd, entry->data, entry->bytes + 2);
					process_stats->bytes_written += sizeof(entry->bytes + 2);
				}
				key = strtok(NULL, WHITESPACE);
			}
			END;
			continue;
		}

		if (strncmp(buffer, "cas ", 4) == 0) {
			ssize_t len;
			cache_entry *entry;
			char readbuffer[CLIENT_BUFFER_SIZE] = {0};
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

			char *cas_unique = strtok(NULL, WHITESPACE);
			if (!cas_unique) {
				ERROR;
				continue;
			}

			len = 0;
			while (len < atoi(bytes)) {
				len += read(client_sockfd, readbuffer + len, sizeof readbuffer - len);
				
			}
			process_stats->bytes_read += len;

			std::lock_guard<std::mutex> guard(map_mutex);
			if ((*map).count(key) == 0) {
				process_stats->cas_misses++;
				NOT_FOUND;
				continue;
			}
			entry = &(*map)[key];

			/* cas stores data only if no one else has updated the data since client read it last */
			if (entry->cas_unique != atoi(cas_unique)) {
				EXISTS;
				process_stats->cas_badval++;
				continue;
			}

			/* 2 is the size of \r\n */
			len -= 2;
			if (len < 1 || len > atoi(bytes)) {
				ERROR;
				CLIENT_ERROR("bad data chunk");
				continue;
			}

			/* delete the key */
			free(entry->data);
			memory_counter -= entry->bytes;
			remove_from_list(entry);
			map->erase(key);
			memory_counter -= sizeof(cache_entry);

			/* create new entry */
			entry = new cache_entry();//(cache_entry*) malloc(sizeof(cache_entry));
			memory_counter += sizeof(cache_entry);
			printf("%s: %u\n", "counter", memory_counter);
			entry->key = key;
			
			if(validate_num(flags) == 0){
				entry->flags = atoi(flags);
			}
			else{
				CLIENT_ERROR("bad command line format");
				continue;
			}
			
			char *exp_temp = expiry;
			if(expiry[0] == '-')
				exp_temp = expiry+1;
				 
			if(validate_num(exp_temp) == 0){
				entry->expiry = atoi(exp_temp);
				
				if(expiry[0] == '-')
					entry->expiry = -(entry->expiry);
			}
			else{
				CLIENT_ERROR("bad command line format");
				continue;
			}
			
			entry->expiry = atoi(expiry);
			
			if(validate_num(bytes) == 0){
				entry->bytes = atoi(bytes);
			}
			else{
				CLIENT_ERROR("bad command line format");
				continue;
			}
			
			entry->cas_unique = generate_cas_unique();
			
			set_expiry(entry);
			
			entry->data = (char*)malloc(entry->bytes + 2);
			memory_counter += entry->bytes;
			memcpy(entry->data, readbuffer, entry->bytes + 2);

			/* CHECK FOR THRESHOLD BREACH */
			printf("%s: %u\n", "counter", memory_counter);
			if (memory_counter > memory_limit) {
				int ret = run_replacement(entry->bytes);
				if (ret) {
					free(entry);
					ERROR;
					SERVER_ERROR("Out of memory");
					continue;
				}
			}
			add_to_list(entry);
			
			process_stats->cas_hits++;
			(*map)[entry->key] = *entry;
			STORED;
			continue;
		}

		if (strncmp(buffer, "delete ", 7) == 0) {
			buffer[strcspn(buffer, "\r\n")] = '\0';
			char *key = strtok(buffer + strlen("delete "), WHITESPACE);

			std::lock_guard<std::mutex> guard(map_mutex);
			if ((*map).count(key) == 0) {
				NOT_FOUND;
				process_stats->delete_misses++;
				continue;
			}
			process_stats->delete_hits++;
			cache_entry *entry = &(*map)[key];
			/* delete the key */
			free(entry->data);
			memory_counter -= entry->bytes;
			remove_from_list(entry);
			map->erase(key);
			memory_counter -= sizeof(cache_entry);
			DELETED;
			continue;
		}

		if (strncmp(buffer, "flush_all", 9) == 0) {

			process_stats->cmd_flush++;

			buffer[strcspn(buffer, "\r\n")] = '\0';
			unsigned error_flag = 0;
			size_t delay = 0;
			char *delay_str = strtok(buffer + strlen("flush_all "), WHITESPACE);

			if (delay_str) {
				for (int i = 0; i < strlen(delay_str); ++i) {
					if (!isdigit(delay_str[i])) {
						error_flag = 1;
						break;
					}
				}
				delay = error_flag ? 0 : atoi(delay_str);
			}
			std::thread flush_all_thread = std::thread(flush_all, delay);
			flush_all_thread.detach();
			OK;
			continue;
		}

		if (strncmp(buffer, "add ", 4) == 0) {
			ssize_t len;
			char *key = strtok((buffer + strlen("add ")), WHITESPACE);
			buffer[strcspn(buffer, "\r\n")] = '\0';

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
			
			std::lock_guard<std::mutex> guard(map_mutex);
			if ((*map).count(key) != 0) {
				NOT_STORED;
				cache_entry *entry = &(*map)[key];
				remove_from_list(entry);
				add_to_list(entry);
				continue;
			}
	
			else {
				cache_entry *entry = new cache_entry();//(cache_entry*) malloc(sizeof(cache_entry));
				memory_counter += sizeof(cache_entry);
				printf("sizeof(cache_entry): %lu\n", sizeof(cache_entry));
				printf("%s: %u\n", "counter", memory_counter);
				entry->key = key;
				
				if(validate_num(flags) == 0){
					entry->flags = atoi(flags);
				}
				else{
					CLIENT_ERROR("bad command line format");
					continue;
				}
			
				char *exp_temp = expiry;
				if(expiry[0] == '-')
					exp_temp = expiry+1;
				 
				if(validate_num(exp_temp) == 0){
					entry->expiry = atoi(exp_temp);
				
					if(expiry[0] == '-')
						entry->expiry = -(entry->expiry);
				}
				else{
					CLIENT_ERROR("bad command line format");
					continue;
				}
				
				entry->expiry = atoi(expiry);

				set_expiry(entry);

				if(validate_num(bytes) == 0){
					entry->bytes = atoi(bytes);
				}
				else{
					CLIENT_ERROR("bad command line format");
					continue;
				}
				entry->cas_unique = generate_cas_unique();

				/* Read actual data and add to map*/
				memset(buffer, 0, sizeof buffer);
				len = 0;
				while (len < entry->bytes) {
					len += read(client_sockfd, buffer + len, sizeof buffer - len);
					
				}
				process_stats->bytes_read += len;

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
				memory_counter += entry->bytes;
				memcpy(entry->data, buffer, entry->bytes + 2);

				//std::lock_guard<std::mutex> guard(map_mutex);
				/* CHECK FOR THRESHOLD BREACH */
				printf("%s: %u\n", "counter", memory_counter);
				if (memory_counter > memory_limit) {
					int ret = run_replacement(entry->bytes);
					if (ret) {
						free(entry);
						ERROR;
						SERVER_ERROR("Out of memory");
						continue;
					}
				}
				add_to_list(entry);

				(*map)[entry->key] = *entry;
				STORED;
				continue;
			}
		}

		if (strncmp(buffer, "replace ", 8) == 0) {
			
			ssize_t len;
			char *key = strtok((buffer + strlen("replace ")), WHITESPACE);
			buffer[strcspn(buffer, "\r\n")] = '\0';

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

			std::lock_guard<std::mutex> guard(map_mutex);
			if ((*map).count(key) != 0) {
	
			cache_entry *entry = new cache_entry();
			if (!entry) {
					ERROR;
					SERVER_ERROR("Out of memory");
					continue;
				}
			memory_counter += sizeof(cache_entry);
			printf("sizeof(cache_entry): %lu\n", sizeof(cache_entry));
			printf("%s: %u\n", "counter", memory_counter);
			entry->key = key;
			//entry->flags = atoi(flags);
			
			if(validate_num(flags) == 0){
				entry->flags = atoi(flags);
			}
			else{
				CLIENT_ERROR("bad command line format");
				continue;
			}
			
			char *exp_temp = expiry;
			if(expiry[0] == '-')
				exp_temp = expiry+1;
				 
			if(validate_num(exp_temp) == 0){
				entry->expiry = atoi(exp_temp);
				
				if(expiry[0] == '-')
					entry->expiry = -(entry->expiry);
			}
			else{
				CLIENT_ERROR("bad command line format");
				continue;
			}
			
			entry->expiry = atoi(expiry);
			set_expiry(entry);

			if(validate_num(bytes) == 0){
				entry->bytes = atoi(bytes);
			}
			else{
				CLIENT_ERROR("bad command line format");
				continue;
			}
			entry->cas_unique = generate_cas_unique();

			/* Read actual data and add to map*/
			memset(buffer, 0, sizeof buffer);
			len = 0;
			while (len < entry->bytes) {
				len += read(client_sockfd, buffer + len, sizeof buffer - len);
			}

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
			memory_counter += entry->bytes;
			memcpy(entry->data, buffer, entry->bytes + 2);
			//std::lock_guard<std::mutex> guard(map_mutex);
			/* CHECK FOR THRESHOLD BREACH */
			printf("%s: %u\n", "counter", memory_counter);

				if (memory_counter > memory_limit) {
					int ret = run_replacement(entry->bytes);
					if (ret) {
						free(entry);
						ERROR;
						SERVER_ERROR("Out of memory");
						continue;
					}
				}
				add_to_list(entry);
				(*map)[entry->key] = *entry;
				STORED;
				continue;
			}
			
			else {
				NOT_STORED;
				continue;
			}		
		}

		if (strncmp(buffer, "append ", 7) == 0) {
			/* resolved */
			ssize_t len;
			char *key = strtok((buffer + strlen("append ")), WHITESPACE);
			if (!key) {
				ERROR;
				continue;
			}

			char *bytes = strtok(NULL, WHITESPACE);
			if (!bytes) {
				ERROR;
				continue;
			}
			if ((*map).count(key) != 0) {
				cache_entry *entry = &(*map)[key];
				memset(buffer, 0, sizeof buffer);
				len = 0;

				/* get len to append */
				len = read(client_sockfd, buffer + len, sizeof buffer - len);
				process_stats->bytes_read += len;
				len -= 2;
				if (len < 1) {
					ERROR;
					CLIENT_ERROR("Nothing added to value");
					continue;
				}

				/* reassign so that bytes is not greater than len */
				
				entry->cas_unique = generate_cas_unique();
				size_t orig_end = entry->bytes;
				entry->bytes = len + entry->bytes;
				
				
				entry->data = (char*) realloc(entry->data, entry->bytes + 2);
				memory_counter += len;
				
				memcpy(entry->data + orig_end, buffer, entry->bytes + 2);

				std::lock_guard<std::mutex> guard(map_mutex);
				/* CHECK FOR THRESHOLD BREACH */
				printf("%s: %u\n", "counter", memory_counter);
				if (memory_counter > memory_limit) {
					int ret = run_replacement(entry->bytes);
					if (ret) {
						free(entry);
						ERROR;
						SERVER_ERROR("Out of memory");
						continue;
					}
				}
				add_to_list(entry);

				(*map)[entry->key] = *entry;
				STORED;
				continue;
			}
			NOT_STORED;
			continue;
		}



		if (strncmp(buffer, "prepend ", 8) == 0) {
			/*RESOLVED*/			
			ssize_t len;
			char *key = strtok((buffer + strlen("prepend ")), WHITESPACE);
			if (!key) {
				ERROR;
				continue;
			}

			char *bytes = strtok(NULL, WHITESPACE);
			if (!bytes) {
				ERROR;
				continue;
			}

			if ((*map).count(key) != 0) {
				cache_entry *entry = &(*map)[key];
				memset(buffer, 0, sizeof buffer);
				len = read(client_sockfd, buffer, sizeof buffer);
				process_stats->bytes_read += len;
				len -= 2;
				if (len < 1) {
					ERROR;
					CLIENT_ERROR("Nothing added to value");
					continue;
				}
				char * temp;
				/* reassign so that bytes is not greater than len */
				entry->cas_unique = generate_cas_unique();
				
				temp = (char*) realloc(entry->data, entry->bytes + (uint32_t)len + 2);
				
				int orig_bytes = entry->bytes;
				entry->bytes += len;
				
				memory_counter += entry->bytes + (uint32_t)len;
				memmove(temp+len, temp, orig_bytes+2);
				memcpy(temp, buffer, len);
				
				std::lock_guard<std::mutex> guard(map_mutex);
				/* CHECK FOR THRESHOLD BREACH */
				printf("%s: %u\n", "counter", memory_counter);
				if (memory_counter > memory_limit) {
					int ret = run_replacement(entry->bytes);
					if (ret) {
						free(entry);
						ERROR;
						SERVER_ERROR("Out of memory");
						continue;
					}
				}
				add_to_list(entry);

				(*map)[entry->key] = *entry;
				STORED;
				continue;
			}
			NOT_STORED;
			continue;
		}

		
		/* TODO: make reusable */
		if (strncmp(buffer, "incr ", 5) == 0) {
			cache_entry *entry;
			char tmpbuffer[CLIENT_BUFFER_SIZE] = {0};
			char *tmp;
			int new_val;
			unsigned error_flag = 0;
			char *key = strtok(buffer + 4, WHITESPACE);
			if (!key) {
				ERROR;
				continue;
			}

			char *delta = strtok(NULL, WHITESPACE);
			if (!delta) {
				ERROR;
				continue;
			}

			for (int i = 0; i < strlen(delta); ++i) {
				if (!isdigit(delta[i])) {
					error_flag = 1;
					break;
				}
			}

			#define MAX_DELTA_LEN 19
			if (error_flag || strlen(delta) > MAX_DELTA_LEN) {
				CLIENT_ERROR("invalid numeric delta argument");
				continue;
			}

			std::lock_guard<std::mutex> guard(map_mutex);
			if ((*map).count(key) == 0) {
				process_stats->incr_misses++;
				NOT_FOUND;
				continue;
			}

			entry = &(*map)[key];
			printf("Value is %s\n", entry->data);
			for (int i = 0; i < entry->bytes; ++i) {
				/* Negative numbers */
				if (i == 0 && entry->data[i] == '-')
					continue;
				if (!isdigit(entry->data[i])) {
					error_flag = 1;
					break;
				}
			}

			if (error_flag) {
				CLIENT_ERROR("cannot increment or decrement non-numeric value");
				continue;
			}

			/* check size of val+delta and see if there needs to be a realloc */
			new_val = atoi(entry->data) + atoi(delta);
			snprintf(tmpbuffer, sizeof tmpbuffer, "%d\r\n", new_val);
			size_t orig_size = entry->bytes;
			entry->bytes = strlen(tmpbuffer) - 2;
			tmp = (char*)realloc(entry->data, entry->bytes + 2);
			if (!tmp) {
				ERROR;
				SERVER_ERROR("Out of memory");
				continue;
			}
			memory_counter += (entry->bytes - orig_size);
			process_stats->incr_hits++;
			entry->data = tmp;
			memcpy(entry->data, tmpbuffer, entry->bytes + 2);
			write(client_sockfd, tmpbuffer, strlen(tmpbuffer));
			process_stats->bytes_written += sizeof(tmpbuffer);
			continue;
		}
		
		if (strncmp(buffer, "decr ", 5) == 0) { 
			cache_entry *entry;
			char tmpbuffer[CLIENT_BUFFER_SIZE] = {0};
			char *tmp;
			int new_val;
			unsigned error_flag = 0;
			char *key = strtok(buffer + 4, WHITESPACE);
			if (!key) {
				ERROR;
				continue;
			}

			char *delta = strtok(NULL, WHITESPACE);
			if (!delta) {
				ERROR;
				continue;
			}

			for (int i = 0; i < strlen(delta); ++i) {
				if (!isdigit(delta[i])) {
					error_flag = 1;
					break;
				}
			}

			#define MAX_DELTA_LEN 19
			if (error_flag || strlen(delta) > MAX_DELTA_LEN) {
				CLIENT_ERROR("invalid numeric delta argument");
				continue;
			}

			std::lock_guard<std::mutex> guard(map_mutex);
			if ((*map).count(key) == 0) {
				process_stats->decr_misses++;
				NOT_FOUND;
				continue;
			}

			entry = &(*map)[key];
			printf("Value is %s\n", entry->data);
			for (int i = 0; i < entry->bytes; ++i) {
				/* Negative numbers */
				if (i == 0 && entry->data[i] == '-')
					continue;
				if (!isdigit(entry->data[i])) {
					error_flag = 1;
					break;
				}
			}

			if (error_flag) {
				CLIENT_ERROR("cannot increment or decrement non-numeric value");
				continue;
			}

			/* check size of val+delta and see if there needs to be a realloc */
			new_val = atoi(entry->data) - atoi(delta);
			snprintf(tmpbuffer, sizeof tmpbuffer, "%d\r\n", new_val);
			size_t orig_size = entry->bytes;
			entry->bytes = strlen(tmpbuffer) - 2;
			tmp = (char*)realloc(entry->data, entry->bytes + 2);
			if (!tmp) {
				ERROR;
				SERVER_ERROR("Out of memory");
				continue;
			}
			memory_counter += (entry->bytes - orig_size);
			process_stats->decr_hits++;
			entry->data = tmp;
			memcpy(entry->data, tmpbuffer, entry->bytes + 2);
			write(client_sockfd, tmpbuffer, strlen(tmpbuffer));
			process_stats->bytes_written += sizeof(tmpbuffer);
			continue;
		}

		if (strncmp(buffer, "stats", 5) == 0) {
			char tmpbuffer[50];
			//char *info = std::strcat("pid ", itoa (process_stats.pid, buf, 10)); 
			//snprintf(tmpbuffer, sizeof tmpbuffer, "%d\r\n", new_val);
			//printf("%d %d\n", process_stats->pid, getpid());
			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT pid %d\r\n", process_stats->pid);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT uptime %ld\r\n", (std::time(NULL)-process_stats->start_time));
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT time %ld\r\n", std::time(NULL));
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT version %s\r\n", process_stats->version);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT pointer_size %d\r\n", process_stats->pointer_size);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT curr_items %d\r\n", process_stats->curr_items);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT total_items %d\r\n", process_stats->total_items);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT curr_connections %d\r\n", process_stats->curr_connections);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT total_connections %d\r\n", process_stats->total_connections);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT cmd_get %lld\r\n", process_stats->cmd_get);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT cmd_set %lld\r\n", process_stats->cmd_set);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT cmd_flush %lld\r\n", process_stats->cmd_flush);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT get_hits %lld\r\n", process_stats->get_hits);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT get_misses %lld\r\n", process_stats->get_misses);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT delete_misses %lld\r\n", process_stats->delete_misses);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT delete_hits %lld\r\n", process_stats->delete_hits);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT incr_misses %lld\r\n", process_stats->incr_misses);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT incr_hits %lld\r\n", process_stats->incr_hits);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT decr_misses %lld\r\n", process_stats->decr_misses);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT decr_hits %lld\r\n", process_stats->decr_hits);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT cas_misses %lld\r\n", process_stats->cas_misses);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT cas_hits %lld\r\n", process_stats->cas_hits);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT cas_badval %lld\r\n", process_stats->cas_badval);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT evictions %lld\r\n", process_stats->evictions);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT bytes_read %lld\r\n", process_stats->bytes_read);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT bytes_written %lld\r\n", process_stats->bytes_written);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			snprintf(tmpbuffer, sizeof tmpbuffer, "STAT limit_maxbytes %d\r\n", process_stats->limit_maxbytes);
			STAT(tmpbuffer);
			memset(&tmpbuffer[0], '\0', sizeof(tmpbuffer));

			END;
			continue;

		}
				

		if (strncmp(buffer, "set ", 4) == 0) {
			ssize_t len;
			process_stats->cmd_set++;
			char *key = strtok((buffer + strlen("set ")), WHITESPACE);
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

			cache_entry *entry = new cache_entry();//(cache_entry*) malloc(sizeof(cache_entry)); //new cache_entry()
			memory_counter += sizeof(cache_entry);
			printf("sizeof(cache_entry): %lu\n", sizeof(cache_entry));
			printf("%s: %u\n", "counter", memory_counter);
			entry->key = key;
			
			if(validate_num(flags) == 0){
				entry->flags = atoi(flags);
			}
			else{
				CLIENT_ERROR("bad command line format");
				continue;
			}
			
			char *exp_temp = expiry;
			if(expiry[0] == '-')
				exp_temp = expiry+1;
				 
			if(validate_num(exp_temp) == 0){
				entry->expiry = atoi(exp_temp);
				
				if(expiry[0] == '-')
					entry->expiry = -(entry->expiry);
			}
			else{
				CLIENT_ERROR("bad command line format");
				continue;
			}
			
			entry->expiry = atoi(expiry);
			
			set_expiry(entry);
			
			if(validate_num(bytes) == 0){
				entry->bytes = atoi(bytes);
			}
			else{
				CLIENT_ERROR("bad command line format");
				continue;
			}
			entry->cas_unique = generate_cas_unique();

			/* Read actual data and add to map*/
			memset(buffer, 0, sizeof buffer);
			len = 0;
			while (len < entry->bytes) {
				len += read(client_sockfd, buffer + len, sizeof buffer - len);
				
			}
			process_stats->bytes_read += len;

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
				CLIENT_ERROR("bad data chunk");
				continue;
			}
			/* reassign so that bytes is not greater than len */
			entry->bytes = (uint32_t)len;
			entry->data = (char*)malloc(entry->bytes + 2);
			memory_counter += entry->bytes;
			memcpy(entry->data, buffer, entry->bytes + 2);

			std::lock_guard<std::mutex> guard(map_mutex);
			/* CHECK FOR THRESHOLD BREACH */
			printf("%s: %u\n", "counter", memory_counter);
			if (memory_counter > memory_limit) {
				int ret = run_replacement(entry->bytes);
				if (ret) {
					free(entry);
					ERROR;
					SERVER_ERROR("Out of memory");
					continue;
				}
			}
			
			if(entry->expiry < 0){
				STORED;
				continue;
			}
			
			add_to_list(entry);

			(*map)[entry->key] = *entry;
			process_stats->curr_items++;
			process_stats->total_items++;
			STORED;
			continue;
		}

		/* default case */
		ERROR;
	}
}
