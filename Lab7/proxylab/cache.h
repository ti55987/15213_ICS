/*
 * cache.h 
 * Name: Ti-Fen Pan
 * Andrew ID: tpan
 * Prototypes and definitions for cache
 */

#ifndef CACHE_H
#define CACHE_H

#define MAX_CACHE_SIZE 1049000


/* Definition of cache node */
typedef struct cachenode
{
	char *id;
    unsigned int _size;
    char *content;
    struct cachenode *next;
    struct cachenode *prev;
}node;

/* Definition of cache list */
typedef struct
{
	unsigned int _size;
	node *head;
	node *tail;
}cache_list;

/* Methods used in proxy.c */
void init_cache_list(cache_list *cl);
void update_cache(cache_list *cl, char *id, char *content,  
				  unsigned int block_size);
void free_cache_list(cache_list *cl);
char* search_cache(cache_list *cl, char *id, int* size);


node *new_cache(char *id, char *content, 
				unsigned int block_size);
void push_to_head(cache_list *cl, node *cb);
node *delete_cache(cache_list *cl, node *cb);
sem_t sem;

#endif