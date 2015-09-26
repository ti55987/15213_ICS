/*
 * cache.c 
 * 
 * Name: Ti-Fen Pan
 * Andrew ID: tpan
 *
 * Overview:
 * Basically, a linked list is used to cache web content.
 * Each cache node contains:
 * [request header]
 * [node size]
 * [the cache content]
 * [pointer to the previous node]
 * [pointer to the next node]
 * To fulfill the LRU policy, I added new cache node at
 * the head of the cache list and remove old node from the
 * tail.  This cache node is moved to the head as a cache hit
 * occurs. Also, I lock the cache each time as I manipulate 
 * the cache node to ensure thread-safe.
 */

#include "csapp.h"
#include "cache.h"




/*
 * Initialize cache list
 */
void init_cache_list(cache_list *cl)
{
	cl->_size = 0;

	/* Create dummy node for head and tail */
	cl->head = new_cache(NULL, NULL, 0);
	cl->tail = new_cache(NULL, NULL, 0);

	cl->head->next = cl->tail;
	cl->tail->prev = cl->head;

	/* initialize lock */
	Sem_init(&sem, 0, 1); 

	return;
}

/*
 * Create a new cache block
 */
node *new_cache(char *id, char *content, 
				unsigned int _size)
{
	node *cb;
	cb = (node *)malloc(sizeof(node));

	/* 
	 * copy cache id, if id == NULL, 
	 * it is header and tail
	 */
	if (id != NULL)
	{
		cb->id = (char *) malloc(sizeof(char) * (strlen(id) + 1));
		strcpy(cb->id, id);
	}

	cb->_size = _size;

	/* 
	 * copy cache content, if content == NULL, 
	 * it is header and tail
	 */
	if (content != NULL)
	{
		cb->content = (char *) malloc(sizeof(char) * _size);
		memcpy(cb->content, content, sizeof(char) * _size);
	}

	cb->prev = NULL;
	cb->next = NULL;

	return cb;
}

/*
 * Insert a cache block to the head of the list
 */
void push_to_head(cache_list *cl, node *cb)
{
	/* manipulate pointer */
	cb->prev = cl->head;
	cb->next = cl->head->next;
	cl->head->next->prev = cb;
	cl->head->next = cb;
	return;
}

/*
 * Delete cache block
 * Return the previous block of the deleted block
 */
node *delete_cache(cache_list *cl, node *cb)
{
	/* remove cache block from list */
	node *prev_cb;
	cb->next->prev = cb->prev;
	cb->prev->next = cb->next;
	cl->_size -= cb->_size;
	prev_cb = cb->prev;

	cb->prev = NULL;
	cb->next = NULL;

	/* Free heap */
	free(cb->id);
	free(cb->content);
	free(cb);

	return prev_cb;
}

/*
 * Free cache list
 */
void free_cache_list(cache_list *cl)
{
	/* delete all cache block */
	node *cb = cl->tail->prev;
	while(cb != cl->head)
	{
		cb = delete_cache(cl, cb);
	}

	/* free heap */
	free(cl->head);
	free(cl->tail);
	free(cl);
	return;
}

/*
 * Check cache list, if there exist the request content,
 * read from it.
 */
char* search_cache(cache_list *cl, char *id, int* size)
{
	node *cache = NULL;

	/* 
	 * When there is cache hit, we first lock 
	 * it for thread safety
	 */
	P(&sem);
	char* content_copy;

	/*
	 * Serach the cache list
	 */
	node *cn = cl->head->next;
	while( cn != cl->tail)
	{
    	if(!strcmp(cn->id, id))
    	{
    		/* Cache hit!!
    		 * Move the cache to the head of the list
    		 */
    		cn->next->prev = cn->prev;
			cn->prev->next = cn->next;
			push_to_head(cl, cn);
    		cache = cn; 
    		break;
    	}
    	cn = cn->next; 			
    }

	/* if cache hit, copy the content */
	if (cache != NULL)
	{
		*size = cache->_size;
		content_copy = (char*) malloc(sizeof(char)*cache->_size);

		memcpy(content_copy, cache->content,
			   sizeof(char) * cache->_size);
		V(&sem);
        return content_copy;

	}
	else
	{
		V(&sem);
		return NULL;
	}
}

/*
 * Write a new cache node to cache list
 */
void update_cache(cache_list *cl, char *id, char *content,
		unsigned int _size)
{
	node *new_cb = NULL;

	/* 
	 * Write operation should lock the cache list
	 * for thread safety
	 */
	P(&sem);
	new_cb = new_cache(id, content, _size);

    /* 
     * Make room for new objects if the cache exceeds
     * maximum cache size.
     */
	node *cn = cl->tail->prev;
    while((cl->_size + new_cb->_size > MAX_CACHE_SIZE) && 
    	    cn != cl->head)
    {
    	cn = delete_cache(cl, cn);
    }
    /* Push the new cache to the head of the list */
    push_to_head(cl, new_cb);

    /* Change total size */
	cl->_size += cn->_size;

    V(&sem);
    return;

}
