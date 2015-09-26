/*
* Name: Ti-Fen Pan
* Andrew ID: tpan
*/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include "cachelab.h"

#define MEM_SIZE  64

typedef enum {false,true} bool;
typedef unsigned long long int addr_t;

typedef struct 
{
	int hits;
	int misses;
	int evicts;
}Result;

typedef struct{
	long long s_bits; /* 2^s = the number of sets*/
	long long b_bits; /* 2^b = block size*/
	int s;
	int b;
	int E; /* number of lines per set */
	int tag_size;

}cache_par;

typedef struct 
{
	bool valid;
	int used_times;
	addr_t tag;
	char* block;	
}Line;

typedef struct 
{
	Line* lines;
}Set;

typedef struct{
	Set* sets;
}Cache;


/* 
 * usage - Display usage info
 */
static void usage(char *cmd) {
    printf("Usage: %s [-hv] [-s <num>] [-E <num>] [-t <file>]\n", cmd);
    printf("Options: \n");
    printf("  -h        Print this help message.\n");
    printf("  -v        Optional verbose flag.\n");
    printf("  -s <num>  Number of set index bits.\n");   
    printf("  -E <num>  Number of line per set.\n");  
    printf("  -b <num>  Number of block offset bits.\n");  
    printf("  -t <file> Trace file .\n");  
    printf("\nExamples:\n");
    printf(" %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", cmd);
    printf(" %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n",cmd);
    exit(1);
}

Cache cacheInit(cache_par p){

	Cache cache;
	Set set;
	Line line;

	cache.sets = (Set*) malloc(p.s_bits *sizeof(Set));
	for(int i = 0; i < p.s_bits ; i++){
		
		set.lines = (Line*) malloc(p.E *sizeof(Line));
		for(int j = 0 ; j < p.E ; j++){
			line.valid = false;
			line.used_times = 0;
			line.tag = 0;
			set.lines[j] = line;
		}

		cache.sets[i] = set;
	}

	return cache;

}

int find_empty(Set set, cache_par p){
	Line line;
	for(int i = 0; i < p.E ; i++){
		line = set.lines[i];
		if(!line.valid) return i;
	}

	return -1;
}

int find_evict(Set set, cache_par p, int* used_lines){
	/*return the index of LRU line
	used_lines[0] = LRU line counts
	used_lines[1] = MRU line counts*/
	int min = set.lines[0].used_times;
	int max = set.lines[0].used_times;
	int min_idx = 0;
	Line line;

	for(int i = 1 ; i < p.E; i++){
		line = set.lines[i];

		if(min > line.used_times){
			min_idx = i;
			min = line.used_times;
		}

		if(max < line.used_times)
			max = line.used_times;
	}

	used_lines[0] = max;
	return min_idx;
}

Result simulate(Cache cache, cache_par p , Result res,addr_t address){

	addr_t tag = address >> (p.s + p.b);
	unsigned long long tmp = address << (p.tag_size);
	unsigned long long setIdx = tmp >> (p.tag_size + p.b);
	bool cache_full = true;
	int prev_hits = res.hits;
	Set oper_set = cache.sets[setIdx];

	for(int i = 0; i < p.E; i++){
		Line oper_line = oper_set.lines[i];

		if(oper_line.valid){
			if(oper_line.tag == tag){
				oper_line.used_times++;
				res.hits++;
				oper_set.lines[i] = oper_line;
			}
		}else if (cache_full) cache_full = false;
	}

	if(prev_hits == res.hits)
		res.misses++;
	else return res;

	/*missing so evict a line if necessary*/
	int *used_lines = (int*) malloc(sizeof(int)*1);
	int evicted_idx = find_evict(oper_set, p, used_lines);

	if(cache_full){
		res.evicts++;
		//Found least recently used (LRU) line
		oper_set.lines[evicted_idx].tag = tag;
		oper_set.lines[evicted_idx].used_times = used_lines[0]+1;
	}else{
		int empty_idx = find_empty(oper_set, p);
		//Found empty line and store the data.
		oper_set.lines[empty_idx].tag = tag;
		oper_set.lines[empty_idx].valid = true;
		oper_set.lines[empty_idx].used_times = used_lines[0]+1;
	}

	free(used_lines);
	return res;

}

void clear_cache(Cache cache, cache_par p){

	for(int i = 0; i < p.s; i++){
		Set set = cache.sets[i];
		if(set.lines != NULL) 
			free(set.lines);
	}

	if(cache.sets != NULL)
		free(cache.sets);

}
/************** 
 * Main routine 
 **************/

int main(int argc, char *argv[])
{
    
    char *file_name;
    FILE *read_file;

    cache_par param;
    Cache simulator;
    Result res;

    addr_t address;
    int size;

    char c;
    char cmd;
    bool verbose = false;

    /* parse command line args */
    while ((c = getopt(argc, argv, "s:E:b:t:vh")) != -1){
        switch (c) {
        case 'h': /* help */
	    	usage(argv[0]);
	    	break;
		case 'v': 
		    verbose = true;
		    break;
		case 's': 
		    param.s = atoi(optarg);
		    break;
		case 'E': 
		    param.E = atoi(optarg);
		    break;
		case 'b': 
		    param.b = atoi(optarg);
		    break;
		case 't': 
		    file_name = optarg;
		    break;
		default:
		    usage(argv[0]);
		    exit(1);
		}
	}

	if(param.s == 0 || param.E == 0 || param.b == 0 || file_name == NULL){
		printf("%s: Missing argument\n", argv[0]);
		usage(argv[0]);
		exit(1);
	}

	param.s_bits = pow(2.0, param.s);
	param.b_bits = pow(2.0, param.b);
	param.tag_size = MEM_SIZE - param.s - param.b;
	res.hits = 0;
	res.misses = 0;
	res.evicts = 0;

	simulator = cacheInit(param);

    read_file = fopen(file_name,"r");

    if(read_file != NULL){
    	while(fscanf(read_file," %c %llx,%d", &cmd, &address, &size) == 3){
    		switch(cmd){
    			case 'L': /* a data load */
    				res=simulate(simulator,param,res,address);
    				break;
    			case 'S': /* a data store */
    				res=simulate(simulator,param,res,address);
    				break;
    			case 'M': /* a data modify = load -> store */
    				res=simulate(simulator,param,res,address);
    				res=simulate(simulator,param,res,address);
					break;
    			default:
    				break;
    		}
    	}
    }

    printSummary(res.hits, res.misses, res.evicts);
    fclose(read_file);
    clear_cache(simulator,param);
    return 0;
}
