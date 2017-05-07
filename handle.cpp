static uint64_t generate_cas_unique(void)
{
	return (int64_t)rand();
}


static void set_expiry(cache_entry *entry){
  if(entry->expiry < 60*60*24*30){
    if(entry->expiry > 0)
      entry->expiry += std::time(NULL);
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

		if (strncmp(buffer, "quit", 4) == 0) {
			close(client_sockfd);
			return;
		}

		if (strncmp(buffer, "version", 4) == 0) {
			VERSION;
			continue;
		}

		/* get and gets */
		if (strncmp(buffer, "get", 3) == 0) {
			unsigned gets_flag = 0;
			if (strncmp(buffer, "gets ", 4) == 0)
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
			        if((*map).count(key) == 0){
			        	track_misses(key);
			        }
				if ((*map).count(key) != 0) {
					cache_entry *entry = &(*map)[key];
					write_VALUE(client_sockfd, entry, gets_flag);
					write(client_sockfd, entry->data, entry->bytes + 2);
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

			std::lock_guard<std::mutex> guard(map_mutex);
			if ((*map).count(key) == 0) {
				NOT_FOUND;
				continue;
			}
			entry = &(*map)[key];

			/* cas stores data only if no one else has updated the data since client read it last */
			if (entry->cas_unique != atoi(cas_unique)) {
				EXISTS;
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
			entry->flags = atoi(flags);
			entry->expiry = atoi(expiry);
			entry->bytes = atoi(bytes);
			entry->cas_unique = generate_cas_unique();
			
			set_expiry(entry);
			
			entry->data = (char*)malloc(entry->bytes + 2);
			memory_counter += entry->bytes;
			memcpy(entry->data, readbuffer, entry->bytes + 2);

			/* CHECK FOR THRESHOLD BREACH */
			printf("%s: %u\n", "counter", memory_counter);
			if (memory_counter > MEMORY_THRESHOLD) {
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

		if (strncmp(buffer, "delete ", 7) == 0) {
			buffer[strcspn(buffer, "\r\n")] = '\0';
			char *key = strtok(buffer + strlen("delete "), WHITESPACE);

			std::lock_guard<std::mutex> guard(map_mutex);
			if ((*map).count(key) == 0) {
				NOT_FOUND;
				continue;
			}
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
			unsigned error_flag = 0;
			buffer[strcspn(buffer, "\r\n")] = '\0';
			size_t delay;
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

				unsigned gets_flag = 0;
				if (strncmp(buffer, "gets ", 4) == 0)
					gets_flag = 1;
				/* Assert not any arbitrary command beginning with get */
				if (strncmp(buffer + strlen("get") + gets_flag, " ", 1) != 0) {
					ERROR;
					continue;
				}

				std::lock_guard<std::mutex> guard(map_mutex);
				while (key) {
					if ((*map).count(key) != 0) {
						NOT_STORED;
						cache_entry *entry = &(*map)[key];	
						remove_from_list(entry);
						add_to_list(entry);
						continue;
					}
					else {

						cache_entry *entry = (cache_entry*) malloc(sizeof(cache_entry));
						memory_counter += sizeof(cache_entry);
						printf("sizeof(cache_entry): %lu\n", sizeof(cache_entry));
						printf("%s: %u\n", "counter", memory_counter);
						entry->key = key;
						entry->flags = atoi(flags);
						entry->expiry = atoi(expiry);
			
						if(entry->expiry < 60*60*24*30){
						  if(entry->expiry > 0)
						    entry->expiry += std::time(NULL);
						}
			
						entry->bytes = atoi(bytes);
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

						std::lock_guard<std::mutex> guard(map_mutex);
						/* CHECK FOR THRESHOLD BREACH */
						printf("%s: %u\n", "counter", memory_counter);
						if (memory_counter > MEMORY_THRESHOLD) {
					
							collect();
					
						   if(MEMORY_THRESHOLD - memory_counter <= (entry->bytes + sizeof(cache_entry)))
						   {       
							  
							int ret = run_replacement(entry->bytes);
							if (ret) {
								free(entry);
								ERROR;
								SERVER_ERROR("Out of memory");
								continue;
							}
				
						   }
						}
						add_to_list(entry);

						(*map)[entry->key] = *entry;
						STORED;
						continue;
					}
					key = strtok(NULL, WHITESPACE);
					}
					
				}



		if (strncmp(buffer, "replace ", 8) == 0) {
		
		
		}

		if (strncmp(buffer, "append ", 7) == 0) {
	
			//No check for flags or exp time
	
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

			while (key) {
				if ((*map).count(key) != 0) {
					cache_entry *entry = &(*map)[key];
					memset(buffer, 0, sizeof buffer);
					len = 0;

					/* get len to append */
					len += read(client_sockfd, buffer + len, sizeof buffer - len);
					len -= 2;
					if (len < 1) {
						ERROR;
						CLIENT_ERROR("Nothing added to value");
						continue;
					}

					/* reassign so that bytes is not greater than len */
					entry->cas_unique = generate_cas_unique();
					entry->bytes = (uint32_t)len + entry->bytes;
					entry->data = (char*) realloc(entry->data, entry->bytes);
					memory_counter += entry->bytes;
					memcpy(entry->data + entry->bytes, buffer, entry->bytes + 2);

					std::lock_guard<std::mutex> guard(map_mutex);
					/* CHECK FOR THRESHOLD BREACH */
					printf("%s: %u\n", "counter", memory_counter);
					if (memory_counter > MEMORY_THRESHOLD) {
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
				key = strtok(NULL, WHITESPACE);
				}
		}



		if (strncmp(buffer, "prepend ", 8) == 0) { 
		
			//No check for flags or exp time
	
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

			while (key) {
				if ((*map).count(key) != 0) {
					cache_entry *entry = &(*map)[key];
					memset(buffer, 0, sizeof buffer);
					len = 0;

					/* get len to append */
					len += read(client_sockfd, buffer + len, sizeof buffer - len);
					len -= 2;
					if (len < 1) {
						ERROR;
						CLIENT_ERROR("Nothing added to value");
						continue;
					}
					char * temp;
					/* reassign so that bytes is not greater than len */
					entry->cas_unique = generate_cas_unique();
					entry->bytes = entry->bytes + (uint32_t)len;
					temp = (char*) realloc(entry->data, entry->bytes);
					memory_counter += entry->bytes;
					memmove(temp,entry->data, entry->bytes);
					entry->data = temp;
					memcpy(entry->bytes + entry->data, buffer, entry->bytes + 2);

					std::lock_guard<std::mutex> guard(map_mutex);
					/* CHECK FOR THRESHOLD BREACH */
					printf("%s: %u\n", "counter", memory_counter);
					if (memory_counter > MEMORY_THRESHOLD) {
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
				key = strtok(NULL, WHITESPACE);
				}

		}

		if (strncmp(buffer, "gets ", 5) == 0) { 


		END;
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
			entry->bytes = strlen(tmpbuffer) - 2;
			tmp = (char*)realloc(entry->data, entry->bytes + 2);
			if (!tmp) {
				ERROR;
				SERVER_ERROR("Out of memory");
				continue;
			}
			entry->data = tmp;
			memcpy(entry->data, tmpbuffer, entry->bytes + 2);
			write(client_sockfd, tmpbuffer, strlen(tmpbuffer));
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
			entry->bytes = strlen(tmpbuffer) - 2;
			tmp = (char*)realloc(entry->data, entry->bytes + 2);
			if (!tmp) {
				ERROR;
				SERVER_ERROR("Out of memory");
				continue;
			}
			entry->data = tmp;
			memcpy(entry->data, tmpbuffer, entry->bytes + 2);
			write(client_sockfd, tmpbuffer, strlen(tmpbuffer));
			continue;
		}

		if (strncmp(buffer, "stats ", 6) == 0) { 

		}
				

		if (strncmp(buffer, "set ", 4) == 0) {
			ssize_t len;
			char *key = strtok((buffer + strlen("set ")), WHITESPACE);
			if (!key) {
				ERROR;
				continue;
			}
			
			track_misses(key);

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

			cache_entry *entry = new cache_entry();//(cache_entry*) malloc(sizeof(cache_entry));
			memory_counter += sizeof(cache_entry);
			printf("sizeof(cache_entry): %lu\n", sizeof(cache_entry));
			printf("%s: %u\n", "counter", memory_counter);
			entry->key = key;
			entry->flags = atoi(flags);
			entry->expiry = atoi(expiry);
			
			set_expiry(entry);
			
			entry->bytes = atoi(bytes);
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

			std::lock_guard<std::mutex> guard(map_mutex);
			/* CHECK FOR THRESHOLD BREACH */
			printf("%s: %u\n", "counter", memory_counter);
			if (memory_counter > MEMORY_THRESHOLD) {
			        
			       // collect();
			        
			   if(memory_counter > MEMORY_THRESHOLD || (MEMORY_THRESHOLD - memory_counter <= (entry->bytes + sizeof(cache_entry))))
			   {       
			          
				int ret = run_replacement(entry->bytes);
				if (ret) {
					free(entry);
					ERROR;
					SERVER_ERROR("Out of memory");
					continue;
				}
				
		           }
			}
			add_to_list(entry);

			(*map)[entry->key] = *entry;
			STORED;
			continue;
		}

		/* default case */
		ERROR;
	}
}
