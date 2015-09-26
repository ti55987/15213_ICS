/*
 * mm.c
 *
 * Name : Ti-Fen Pan
 * Andrew ID: tpan
 * 
 * Overview:
 * I used segregatede free list and bineray search tree.
 * All free blocks in one list have the same size. 
 * There are two types of blocks:
 * 1.) Small-size free blocks: being stored in segregate lists.
 *
 * 2.) Large-size free blocks: being stored in binary search tree.
 * (with block_size larger than 40 bytes)
 *
 * binary search tree (BST) structure:
 *     The block_size of the left_child is less than that of the parent.
 *     The block_size of the right_child is larger than that of the parent.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

#include <linux/kernel.h>
#include <linux/stddef.h>

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
 /*
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif
*/
 #ifdef DEBUG
 static int operid;
 #endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */


typedef void *blkp;

/* Global Variables*/
static char *heap_listp = 0; /*Pointer to the first block*/
static blkp large_blkroot;  /*Root of the BST in large block*/
static blkp *header_arr; /*Array of the headers of segregated lists*/
static const unsigned int ARRAY_NUM = 5; /* Number of segregated free lists */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((p) + (ALIGNMENT-1)) & ~0x7)

 #define WSIZE		4		 /* Word and header/footer size (bytes) */
 #define DSIZE		8		 /* Double word size (bytes)*/
 #define BLOCKSIZE	(1 << 6) /* Extend heap by this amount (bytes) */

 #define MAX(x,y)	( (x) > (y) ? (x) : (y))

 /* Pack a size and allocated bit into a word */
 #define PACK(size, alloc) ((size) | (alloc))

 /* Read and write a word at address p */
 #define GET(p)				(* (unsigned int *) (p))
 #define INIT_PUT(p, val)	(* (unsigned int *) (p) = (val))
 #define PUT(p, val) 		(GET(p) = (GET(p) & 0x2) | (val))

 /* Read the size and allocated fields from address */
 #define GET_SIZE(p) 	(GET(p) & ~0x7)
 #define GET_ALLOC(p)	(GET(p) & 0x1)

 /* Return address of current pointer's header and footer */
 #define HDRP(bp)		((char *)(bp) - WSIZE)
 #define FTRP(bp)		((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Return address of current pointer's next and previous blocks */
 #define NEXT_BLKP(bp)	((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
 #define PREV_BLKP(bp)	((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block ptr bp, set next block's prev_alloc */
 #define SET_NEXT_ALLOC(bp)	    (GET(HDRP(NEXT_BLKP(bp))) |= 0x2)
 #define SET_NEXT_UNALLOC(bp)	(GET(HDRP(NEXT_BLKP(bp))) &= ~0x2)
 #define GET_PREV_ALLOC(bp) 	(GET(HDRP(bp)) & 0x2)

 /* Given a freed block ptr bp, compute 'address' of previous blocks of same size */
 #define PRE_SAMESZ_BLKP(bp)    (*(unsigned int *)((char *)(bp) + WSIZE))



/* Check if the block is large enough to be a BST node*/
 #define GT_BST_SIZE(size)	((size) > DSIZE * ARRAY_NUM)
 #define IS_BST_NODE(bp)	(GT_BST_SIZE(GET_SIZE(HDRP(bp))))

 #define LCHLD_BLKP(bp)   ((blkp *)((char *)(bp) + DSIZE))
 #define RCHLD_BLKP(bp)   ((blkp *)((char *)(bp) + DSIZE * 2))
/*Pointer to the pointer to itself in its parent block in BST*/
 #define PARENT_CHILDP(bp) ((blkp **)((char *)(bp) + DSIZE * 3))



/* Convert 4-byte address to 8-byte address */
static inline blkp word_to_ptr(unsigned int w){
    return ((w) == 0U ? NULL : (blkp)((unsigned int)(w) + 0x800000000UL));
}

/* Convert 8-byte address to 4-byte address */
static inline unsigned int ptr_to_word(blkp p){
    return((p) == NULL ? 0U : (unsigned int)((unsigned long)(p) - 0x800000000UL));
}

/* Reset the fields of a free block bp */
#define reset_blk(bp)                                                        \
{                                                                            \
    GET(bp) = 0U;                                           				 \
    PRE_SAMESZ_BLKP(bp) = 0U;                                          \
    if (IS_BST_NODE(bp))                                                     \
    {                                                                        \
        *LCHLD_BLKP(bp) = NULL;                                              \
        *RCHLD_BLKP(bp) = NULL;                                              \
        *PARENT_CHILDP(bp) = NULL;                                        	 \
    }                                                                        \
}

/* Assign child of deleted node to parent's child of deleted node in BST 
 * rm_node : the deleted node
 * next_node: children of the delted node
 */
#define change_parent_child(rm_node,  next_node){							 \
	*PARENT_CHILDP(next_node) = *PARENT_CHILDP(rm_node);					 \
	**PARENT_CHILDP(rm_node) = next_node;									 \
}

/* Change a node's parent */
#define change_blk_child(node , child){										 \
	if((*node = child))														 \
		*PARENT_CHILDP(child) = node;										 \
}

/* Remove bp from its free list */
static inline void remove_linked_free_blk(blkp bp){                                                                            
    /* Assign the next pointer of its previous node to its next pointer */	 
    if (PRE_SAMESZ_BLKP(bp))                                                 
        GET(word_to_ptr(PRE_SAMESZ_BLKP(bp))) = GET(bp); 

    /* Assign the pre pointer of its next node to its previous pointer */    
    if (GET(bp))                                                       		 
        PRE_SAMESZ_BLKP(word_to_ptr(GET(bp))) = PRE_SAMESZ_BLKP(bp); 		 
}

/* Remove bp from BST if exists and remove it from linked list as well */
static inline void remove_free_blk(blkp bp){

	if(IS_BST_NODE(bp) && *PARENT_CHILDP(bp)){

		blkp l = *LCHLD_BLKP(bp);
		blkp r = *RCHLD_BLKP(bp);
		blkp cur;
		if((cur = word_to_ptr(GET(bp)))){
			/* Assign the freed block parent's child to its next block */
			change_parent_child(bp, cur);
			change_blk_child(LCHLD_BLKP(cur) ,l);
			change_blk_child(RCHLD_BLKP(cur) ,r);

		}else if( l && r){
			/* Find left-most node in right branch to replace curr */
			if(!(cur = *LCHLD_BLKP(r))){
				/* Right child doesn't have lchild */				
				change_parent_child(bp, r);
				change_blk_child(LCHLD_BLKP(r) ,l);

			}else{
				while(*LCHLD_BLKP(cur))
					cur = *LCHLD_BLKP(cur);

				**PARENT_CHILDP(bp) = cur;
				**PARENT_CHILDP(cur) = *RCHLD_BLKP(cur);
				if(*RCHLD_BLKP(cur))
					*PARENT_CHILDP(*RCHLD_BLKP(cur)) = *PARENT_CHILDP(cur);

				*PARENT_CHILDP(cur) = *PARENT_CHILDP(bp);
				change_blk_child(LCHLD_BLKP(cur) ,l);
				change_blk_child(RCHLD_BLKP(cur) ,r);

			}
		}else if(r){
			change_parent_child(bp, r);
		}else if(l){
			change_parent_child(bp, l);
		}else{
			**PARENT_CHILDP(bp) = NULL;
		}

	}else if (!PRE_SAMESZ_BLKP(bp))
		header_arr[GET_SIZE(HDRP(bp)) / DSIZE -1]= word_to_ptr(GET(bp));

	remove_linked_free_blk(bp);
}

 /* Function prototypes */
static blkp coalesce(blkp bp);
static blkp extend_heap(size_t words);
static void place(blkp bp, size_t asize);
static void insert_free_blk(blkp bp, size_t blocksize);
static blkp find_fit(size_t asize);
static int in_heap(const blkp p);
static int aligned(const blkp p);
static void printblock(blkp bp);
static void checkblock(blkp bp);
/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
	/* Create the initial empty heap */
	header_arr = mem_sbrk(
		ALIGN(ARRAY_NUM * sizeof(blkp))+ 4 * WSIZE);

	if(header_arr == (blkp) - 1)
		return -1;
	memset(header_arr, 0, ARRAY_NUM * sizeof(blkp ));
	heap_listp = (char *)ALIGN((unsigned long)(header_arr + ARRAY_NUM));
	large_blkroot = NULL;

	INIT_PUT(heap_listp, 0);								/* Alignment padding */
	INIT_PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));	/* Prologue header */
	INIT_PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));	/* Prologue footer */
	INIT_PUT(heap_listp + (3 * WSIZE), PACK(0,1));		/* Epilogue header */
	heap_listp += (2 *WSIZE);

	SET_NEXT_ALLOC(heap_listp);
	#ifdef DEBUG
    {
        printblock(heap_listp);
    }
    operid = 0;
	#endif
    return 0;
}
/*
 * Boundary tag coalescing. Retrun pointer to a coalesced block
 */
 static blkp coalesce(blkp bp){
 	size_t prev_alloc = GET_PREV_ALLOC(bp);
 	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
 	size_t size = GET_SIZE(HDRP(bp));

 	if( prev_alloc && next_alloc ){
 	/* If previous and next blocks are both allocated, 
 	 * then we don't have to coalesce.
 	 */
 		return bp;

 	}else if ( prev_alloc && !next_alloc ){
 		/* Coalesce the current and next block*/
 		remove_free_blk(NEXT_BLKP(bp));
 		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
 		PUT(HDRP(bp), PACK(size, 0));
 		PUT(FTRP(bp), PACK(size, 0));
 		
 	}else if ( !prev_alloc && next_alloc){
 		/* Coalesce the current and prev block */
 		remove_free_blk(PREV_BLKP(bp));
 		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
 		PUT(FTRP(bp), PACK(size, 0));
 		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
 		bp = PREV_BLKP(bp);

 	}else{
 		/* Remove the next and previous block from its free list */
 		remove_free_blk(NEXT_BLKP(bp));
 		remove_free_blk(PREV_BLKP(bp));
 		/* Coalesce the current, prev and next block */
 		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
 				GET_SIZE(FTRP(NEXT_BLKP(bp)));
 		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
 		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
 		bp = PREV_BLKP(bp);
 	}
 	reset_blk(bp);
 	return bp;
 }

/*
 * Extend heap with free block and return its block pointer
 */
 static blkp extend_heap(size_t words){

 	char *bp;
 	size_t size;

 	/* Allocate an even number of words to maintain alignment */
 	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
 	if((long)(bp = mem_sbrk(size)) == -1)
 		return NULL;
 	
 	#ifdef DEBUG
    	printf("\nExtended the heap by %zu words.\n", words);
	#endif
 	/* Intitialize free block header/footer and he epilogue header */
 	PUT(HDRP(bp), PACK(size, 0)); 			/* Free block header */
	PUT(FTRP(bp), PACK(size, 0));			/* Free block footer */
	reset_blk(bp);
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));	/* New epilogue header */

	/* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * Search for a block with requested size or larger in BST.
 */
 blkp *binary_search(blkp *node, size_t size){
 	
 	blkp *candidate;
 	blkp curr = *node;
 	size_t curr_size;

 	if(curr == NULL)
 		return NULL;

 	curr_size = GET_SIZE(HDRP(curr));

 	if(size == curr_size){
 		return node;
 	}else if(size < curr_size){
 		if((candidate = binary_search(LCHLD_BLKP(curr), size)))
 			return candidate;
 		return node;
 	}else{
 		return binary_search(RCHLD_BLKP(curr), size);
 	}
 }

/*
 * Find a best fit for a block with asize bytes 
 * and asize should be duplicate of double word.
 */
static blkp find_fit(size_t asize){

	size_t dcount = asize / DSIZE;
	blkp curr;
	blkp *blocks;

	if(!GT_BST_SIZE(asize)){
		if(header_arr[dcount - 1]){
			/* Find a free list with the same size */
			curr = header_arr[dcount - 1];
			/* Remove the first node of the list */
			header_arr[dcount - 1] = word_to_ptr(GET(curr));
			remove_free_blk(curr);
			return curr;
		}
	}
	
	if((blocks =binary_search(&large_blkroot, asize)) == NULL)
		return NULL;
	
	curr = *blocks;
	/* Set the node to the next same size block */
	*blocks = word_to_ptr(GET(curr));
	remove_free_blk(curr);

	return curr;
}

/*
 * Insert a block into BST or segregated free list
 */
static void insert_free_blk(blkp bp, size_t blocksize){

	blkp *new = &large_blkroot;
	blkp parent = NULL;
	size_t dcount = blocksize / DSIZE;
	if(!GT_BST_SIZE(blocksize)){

		/* Insert into segregated free list */
		if(header_arr[dcount - 1]){
			GET(bp) = ptr_to_word(header_arr[dcount - 1]);
			PRE_SAMESZ_BLKP(header_arr[dcount - 1]) = ptr_to_word(bp);
		}
		PRE_SAMESZ_BLKP(bp) = 0U;
		header_arr[dcount - 1] = bp;
		return;
	}

	/* Put the new node in BST*/
	while(*new){

		size_t curr_size = GET_SIZE(HDRP(parent = *new));
		if(blocksize < curr_size)
			new = LCHLD_BLKP(parent);
		else if (blocksize > curr_size)
			new = RCHLD_BLKP(parent);
		else{
			blkp nxt = word_to_ptr(GET(bp) = GET(parent));
			if(nxt)
				PRE_SAMESZ_BLKP(nxt) = ptr_to_word(bp);

			GET(parent) = ptr_to_word(bp);
			PRE_SAMESZ_BLKP(bp) = ptr_to_word(parent);
			return;
		}
	}

	*new = bp;
	*PARENT_CHILDP(bp) = new;
	#ifdef DEBUG
    {
        printf("Insert a block: ");
        printblock(bp);
    }
	#endif
}
/*
* Place block of asize bytes at start of free block and
* split if remainder would be at least minimum block size
*/
static void place(blkp bp, size_t asize){

	size_t csize = GET_SIZE(HDRP(bp));
	size_t del = csize - asize;

	if(del >= (2*DSIZE)){
		/* 
		 * The remainding size is greather or equal 
		 * than twice double size, then split the block.
		 */
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		SET_NEXT_ALLOC(bp);
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(del, 0));
		PUT(FTRP(bp), PACK(del, 0));
		SET_NEXT_UNALLOC(bp);
		reset_blk(bp);
		insert_free_blk(bp, del);
		#ifdef DEBUG
        {
            printf("Block with size %zu remains a block:\n", asize);
            printblock(bp);
        }
		#endif
	}else{
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		SET_NEXT_ALLOC(bp);
	}
}

/*	
 * malloc
 */
blkp malloc (size_t size) {

	size_t asize; /* Adjusted block size */
	size_t extendsize;	/* Amount to extend heap if no fit */
	char *bp;

	/*Ignore spurious requests */
	if(size == 0)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	if(size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + (WSIZE) + (DSIZE - 1)) / DSIZE);

	if(heap_listp == 0)
		mm_init();

	#ifdef DEBUG
    	printf("\nMalloc request: size = %zu, rounded to %zu \033[41;37m[ID:%d]\033[0m\n", size, asize, operid++);
	#endif
	/* Search the free list for a fit */
	if((bp = find_fit(asize)) != NULL){
		#ifdef DEBUG
        {
        	checkblock(bp);
            printblock(bp);
        }
		#endif
		place(bp, asize);
		return bp;
	}

	/* No fit found. Get more memory and place the block */
	extendsize = MAX(asize, BLOCKSIZE);
	if((bp = extend_heap(extendsize / WSIZE)) == NULL)
		return NULL;
	place(bp, asize);
	return bp;
}

/*
 * free
 */
void free (blkp ptr) {

	blkp tmp;
	size_t size;
    if(!ptr || !in_heap(ptr) || !aligned(ptr)) 
    	return;
    #ifdef DEBUG
    {
        printf("\nFree request: ptr = %p \033[41;37m[ID:%d]\033[0m\n", ptr, operid++);
        printblock(ptr);
    }
	#endif
    size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    SET_NEXT_UNALLOC(ptr);
    reset_blk(ptr);
    tmp = coalesce(ptr);
    insert_free_blk(tmp, GET_SIZE(HDRP(tmp)));
}

/*
 * realloc - you may want to look at mm-naive.c
 */
blkp realloc(blkp oldptr, size_t size) {

	size_t oldsize;
	blkp newptr;

	/* If the size == 0 which means it is just free
	 * ,and we return NULL.
	 */
	 if(size == 0){
	 	free(oldptr);
	 	return 0;
	 }
	 /* If oldptr is NULL, this equals to malloc */
	 if(oldptr == NULL){
	 	return malloc(size);
	 }

	 newptr = malloc(size);

	 /* If relloc() fails the original block is left untouched */
	 if(!newptr){
	 	return 0;
	 }

	/* Copy the old data. */
	oldsize = GET_SIZE(HDRP(oldptr));
	if(size < oldsize)
	 	oldsize = size;
	memcpy(newptr, oldptr, oldsize);

	/* Free the old block */
	free(oldptr);

    return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
blkp calloc (size_t nmemb, size_t size) {

	size_t bytes = nmemb * size;
	blkp newptr;

	newptr = malloc(bytes);
	memset(newptr, 0, bytes);

    return newptr;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static inline int in_heap(const blkp p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static inline int aligned(const blkp p) {
    return ((unsigned long)p % ALIGNMENT) == 0;
}

static inline void printblock(blkp bp)
{

    size_t hsize = GET_SIZE(HDRP(bp));
    size_t fsize = GET_SIZE(FTRP(bp));
    size_t halloc = GET_ALLOC(HDRP(bp));
    size_t falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0){
        printf("%p: EOL\n", bp);
        return;
    }

    if (halloc){
        printf("%p: header: [%zu:%c:%c] footer: -\n", bp,
        hsize, (GET_PREV_ALLOC(bp) ? 'a' : 'f'), (halloc ? 'a' : 'f'));
    }else{
        printf("%p: header: [%zu:%c:%c] footer: [%zu:%c]", bp,
            hsize, (GET_PREV_ALLOC(bp) ? 'a' : 'f'), (halloc ? 'a' : 'f'),
            fsize, (falloc ? 'a' : 'f'));
        if (IS_BST_NODE(bp))
            printf("[BST Node| parent slotp: %p, l: %p, r: %p]",
            *PARENT_CHILDP(bp), *LCHLD_BLKP(bp), *RCHLD_BLKP(bp));
        if (PRE_SAMESZ_BLKP(bp))
            printf("[PREV] %p", word_to_ptr(PRE_SAMESZ_BLKP(bp)));   
    }
}
/*
 * checkblock - as the name goes
 */
static inline void checkblock(blkp bp)
{
    if (!aligned(bp))
        printf("\nError: %p is not aligned\n", bp);
    if (!GET_ALLOC(HDRP(bp)) && (GET(HDRP(bp)) & ~0x2) != (GET(FTRP(bp)) & ~0x2))
        printf("\nError: header does not match footer, header: %u, footer: %u \n",
        GET(HDRP(bp)), GET(FTRP(bp)));
    if (GET_ALLOC(HDRP(bp)) != (GET_PREV_ALLOC(NEXT_BLKP(bp)) >> 1))
        printf("\n Error: %p allocation does not match next block's prev_alloc\n", bp);
}
/*
 * mm_checkheap
 * Check for consistency
 */
void mm_checkheap(int lineno) {

	char *bp = heap_listp;

	if(lineno)
		printf("Heap (%p):\n", heap_listp);

	if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("\n\033[1;47;31m## Bad prologue header\033[0m\n");
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (lineno)
            printblock(bp);
        checkblock(bp);
    }
}
