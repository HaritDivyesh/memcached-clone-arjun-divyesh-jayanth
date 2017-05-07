static void sweep(node_t *node){
  std::cout<<"In sweeper";
  if(node->prev)
    node->prev->next = node->next;
    
  if(node->next)
    node->next->prev = node->prev;
  
  int cleared = node->entry->bytes + sizeof(cache_entry);
  memory_counter -= cleared;  
  map->erase(node->entry->key);
  free(node);
  
  //return cleared;
}

static void collect(){
  node_t *temp = head;
  std::cout<<"In collector1";
  while(temp != NULL){
    std::cout<<"In collector2";
    node_t *next_node = temp->next;
    time_t current_time = std::time(NULL);
    int cleared = 0;
    if(temp->entry->expiry <= current_time && temp->entry->expiry != 0)
      sweep(temp);
      
    temp = next_node; 
  }
  
  //return cleared;
}

static void trigger_collection(){
  while(1){
    std::this_thread::sleep_for (std::chrono::seconds(5));
    collect();
  }
}
