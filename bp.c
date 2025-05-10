/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <stdio.h>
#include <stdlib.h>

#define SUCCESS 0
#define FAILURE -1

enum state : char { // fsm state was defined as an unsigned int by course staff
    SNT = 0,
    WNT = 1 , 
    WT = 2,
    ST = 3
};

enum share : int{ // for "Shared"
    NONE = 0,
    LSB = 1, 
    MID =2
};

struct btb{ // Our own auxiliary data structure
	unsigned btbSize;
	unsigned historySize;
	unsigned tagSize;
	unsigned fsmState;
	bool isGlobalHist;
	bool isGlobalTable;
	int Shared;

	unsigned *tag; // array of tags
	char **history; // array of pointers to history char
	char **fsm; // array of pointers to fsm unsigned int arrays
	uint32_t *pred_dst; // arrays of addresses
	// unsigned flush_num;  ---> used in SIM_stats
	// unsigned br_num;    ---> used in SIM_stats
};

struct btb *bp; // Instance of the branch prediction unit
SIM_stats stats; // Instance of the simulator stats

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){
	// Initialize the branch predictor
	bp = (struct btb*) malloc(sizeof(struct btb));
	if (bp == NULL) {
		return FAILURE;
	}
	// Initialize the branch predictor parameters
	bp->btbSize = btbSize;
	bp->historySize = historySize;
	bp->tagSize = tagSize;
	bp->fsmState = fsmState;
	bp->isGlobalHist = isGlobalHist;
	bp->isGlobalTable = isGlobalTable;
	bp->Shared = Shared;


	int tableSize = (1<<historySize)-1; //FSM table size is 2^historySize - 1

	// Initialize the branch predictor arrays: tag, history, fsm, and pred_dst
	bp->tag = malloc(btbSize * sizeof(*(bp->tag));
	if (bp->tag == NULL) {
		free(bp);
		return FAILURE;
	}

	bp->history = malloc(btbSize * sizeof(*(bp->history)));
	if (bp->history == NULL) {
		free(bp->tag);
		free(bp);
		return FAILURE;
	}

	bp->fsm = malloc(btbSize * sizeof(*(bp->fsm)));
	if (bp->fsm == NULL) {
		free(bp->history);
		free(bp->tag);
		free(bp);
		return FAILURE;
	}

	bp->pred_dst = malloc(btbSize * sizeof(*(bp->pred_dst)));
	if (bp->pred_dst == NULL) {
		free(bp->fsm);
		free(bp->history);
		free(bp->tag);
		free(bp);
		return FAILURE;
	}



	for (unsigned i = 0; i < btbSize; i++) {
		bp->tag[i] = 0;
		
		if(isGlobalHist && i != 0){ //Global history - all pointers direct to the same history
		    bp->history[i] = bp->history[0];
		}
		else{ //Local history - multiple allocations are needed
		    bp->history[i] = malloc(sizeof(char));
		}
		if (bp->history[i] == NULL) {// Handle Allocation Errors
			for (unsigned j = 0; j < i && !isGlobalHist ; j++) {
				free(bp->history[j]);
			}
			free(bp->pred_dst);
			free(bp->fsm);
			free(bp->history);
			free(bp->tag);
			free(bp);
			return FAILURE;
		}
		
	    if(isGlobalTable && i != 0){ //Global fsm table - all pointers direct to the same history
		    bp->fsm[i] = bp->fsm[0];
		}
		else{ //Local fsm table - multiple allocations are needed
		    bp->fsm[i] = malloc(tableSize * sizeof(char));
		    for(unsigned j = 0 ; j < i ; j++){
		        *(bp->fsm[i]+j) = fsmState; // initial default state is being set
		    }
		}
		// Handle Allocation Errors
		if (bp->fsm[i] == NULL) {
			for (unsigned j = 0; j < i && !isGlobalTable; j++) {
				free(bp->fsm[j]);
			}
			for (unsigned j = 0 ; j < i && !isGlobalHist; j++) {
			    free(bp->history[j]);
			}
			free(bp->pred_dst);
			free(bp->fsm);
			free(bp->history);
			free(bp->tag);
			free(bp);
			return FAILURE;
		}
		bp->pred_dst[i] = 0;
	}
	
    return SUCCESS; //At last...
}

bool BP_predict(uint32_t pc, uint32_t *dst){
    unsigned index = pc%(bp->btbSize+2)>>2; // the corresponding row in the btb
    if(bp->tag[index] != (pc%(bp->tagSize+2)>>2)){
        //replace the current tag
    }
    if(!bp->Shared || !bp->isGlobalTable){
        
    }
    else{
        
    }
	return false;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	return;
}

void BP_GetStats(SIM_stats *curStats){
	/*simulation stats*/
	/*free*/
	return;
}
