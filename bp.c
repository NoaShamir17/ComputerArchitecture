/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SUCCESS 0
#define FAILURE -1

#define ADDRESS_SIZE 32 //as defined in the assignment's document
#define ADDRESS_JUMP 4 //addresses jump in multiples of 4

enum state { // fsm state was defined as an unsigned int by course staff
    SNT = 0,
    WNT = 1 , 
    WT = 2,
    ST = 3
};

enum share { // for "Shared"
    NONE = 0,
    LSB = 1, 
    MID =2
};

enum pred {
    TAKEN = 1, // branch taken
    NTAKE = 0 //branch not taken
};

struct btb{ // Our own auxiliary data structure
	unsigned btbSize;
	unsigned historySize;
	unsigned tagSize;
	unsigned fsmState;
	bool isGlobalHist;
	bool isGlobalTable;
	int Shared;
    
    bool *used; //checks weather the given index is used in the btb 
	unsigned *tag; // array of tags
	char **history; // array of pointers to history char
	char **fsm; // array of pointers to fsm unsigned int arrays
	uint32_t *pred_dst; // arrays of addresses
};

struct btb *bp; // Instance of the branch prediction unit
bool prediction;
SIM_stats stats = {0, 0, 0}; // Instance of the simulator stats

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


	int tableSize = (1<<historySize); //FSM table size is 2^historySize - 1
    
    //Check Input-Arguments' Validity
    if(Shared && !isGlobalTable){
        return FAILURE;
    }
    
	// Initialize the branch predictor arrays: tag, history, fsm, and pred_dst
	bp->used = malloc(btbSize * sizeof(*(bp->used)));
	if (bp->used == NULL) {
		free(bp);
		return FAILURE;
	}
	
	bp->tag = malloc(btbSize * sizeof(*(bp->tag)));
	if (bp->tag == NULL) {
	    free(bp->used);
		free(bp);
		return FAILURE;
	}

	bp->history = malloc(btbSize * sizeof(*(bp->history)));
	if (bp->history == NULL) {
		free(bp->tag);
		free(bp->used);
		free(bp);
		return FAILURE;
	}

	bp->fsm = malloc(btbSize * sizeof(*(bp->fsm)));
	if (bp->fsm == NULL) {
		free(bp->history);
		free(bp->tag);
		free(bp->used);
		free(bp);
		return FAILURE;
	}

	bp->pred_dst = malloc(btbSize * sizeof(*(bp->pred_dst)));
	if (bp->pred_dst == NULL) {
		free(bp->fsm);
		free(bp->history);
		free(bp->tag);
		free(bp->used);
		free(bp);
		return FAILURE;
	}



	for (unsigned i = 0; i < btbSize; i++) {
		bp->used[i] = false;
		
		if(isGlobalHist && i != 0){ //Global history - all pointers direct to the same history
		    bp->history[i] = bp->history[0];
		}
		else{ //Local history - multiple allocations are needed
		    bp->history[i] = malloc(sizeof(char));
		    *(bp->history[i])=0;
		}
		if (bp->history[i] == NULL) {// Handle Allocation Errors
			for (unsigned j = 0; j < i && !isGlobalHist ; j++) {
				free(bp->history[j]);
			}
			free(bp->pred_dst);
			free(bp->fsm);
			free(bp->history);
			free(bp->tag);
    		free(bp->used);
			free(bp);
			return FAILURE;
		}
		
	    if(isGlobalTable && i != 0){ //Global fsm table - all pointers direct to the same history
		    bp->fsm[i] = bp->fsm[0];
		}
		else{ //Local fsm table - multiple allocations are needed
		    bp->fsm[i] = malloc(tableSize * sizeof(char));
		    for(unsigned j = 0 ; j < tableSize ; j++){
		        *(bp->fsm[i]+j) = fsmState; // initial default state is being set
		    }
		}
		// Handle Allocation Errors
		if (bp->fsm[i] == NULL) {
			for (unsigned j = 0; j < i ; j++) {
				free(bp->fsm[j]);
				if(bp->isGlobalTable) {
	                break;
	            }
			}
			for (unsigned j = 0 ; j < btbSize ; j++) {
			    free(bp->history[j]);
			    if(bp->isGlobalHist){
			        break;
			    }
			}
			free(bp->pred_dst);
			free(bp->fsm);
			free(bp->history);
			free(bp->tag);
			free(bp->used);
			free(bp);
			return FAILURE;
		}
		bp->pred_dst[i] = 0;
	}
    
    return SUCCESS; //At last...
}

int calcTableIndex(uint32_t pc){
    int btb_idx = (pc/ADDRESS_JUMP)%(bp->btbSize);
    int history = (int)*(bp->history[btb_idx]);
	int cleaning_mask = (1 << bp->historySize) - 1; //cleans biths higher than the history size
	history = history & cleaning_mask;
	int hashing_mask;
	switch(bp->Shared){
		case NONE:
		    //not using share
			return history; //history is used as the index in the fsm table
		case LSB:
		    //shared lsb
			hashing_mask = (pc>>2) & cleaning_mask;
            break;
		case MID:
		    //shared mid
			hashing_mask = (pc >> (ADDRESS_SIZE/2) ) & cleaning_mask;
			break;
		default: //should never get here!
			exit(1);
	}
	// NOTE that L-Share and G-Share has no differnet implementation in here
	// due to usage of pointers in "history"
	return history ^ hashing_mask;
}

bool BP_predict(uint32_t pc, uint32_t *dst){
    unsigned btb_idx = (pc>>2)%(bp->btbSize); // the corresponding row in the btb
    int btb_idx_bits = (int)ceil(log2((double)bp->btbSize)); // number of bits needed to address the btb row
    unsigned new_tag = (pc>>(2 + btb_idx_bits))%(1<<bp->tagSize);
    
    //Trivial Cases
    if(!bp->used[btb_idx]){
        *dst = pc+4;
        prediction = NTAKE; //default
        return prediction;
    }
    if(bp->tag[btb_idx] != new_tag){ //old tag is different than the new incoming tag
        *dst = pc+4;
        prediction = NTAKE; //default
        return prediction;
    }
    
    int table_idx = calcTableIndex(pc);
    switch ((bp->fsm[btb_idx])[table_idx]){
        case SNT:
            *dst = pc+4;
            prediction = NTAKE;
            break;
        case WNT:
            *dst = pc+4;
            prediction = NTAKE;
            break;
        case WT:
            *dst = bp->pred_dst[btb_idx];
            prediction = TAKEN;
            break;
        case ST:
            *dst = bp->pred_dst[btb_idx];
            prediction = TAKEN;
            break;
        default:
            exit(1); //should never happen
    }
    return prediction;
    
}
char update_history(char curr_history, bool taken, unsigned historySize);
void update_fsm(char *fsm, bool taken);

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
    //update stats
    stats.br_num++;//num of calls to update
    if((prediction != taken) || ((targetPc != pred_dst) && (prediction == TAKEN))){ //flushes if the predicted destination is not equal to the actual destination
        stats.flush_num ++;
    }
    //extract the tag and index from the pc
    int btb_row_bits = (int)ceil(log2((double)bp->btbSize)); // number of bits needed to address the btb row
    unsigned btb_idx = (pc/ADDRESS_JUMP)%(bp->btbSize); // the corresponding row in the btb
    unsigned curr_tag = (pc>>(btb_row_bits + 2))%(1<<bp->tagSize); // the current tag
    int tableSize = (1<<bp->historySize)-1; //FSM table size is 2^historySize - 1
    
    bp->used[btb_idx] = true;
    if(bp->tag[btb_idx] != curr_tag){
            //new branch - initialize the btb row
            //update the tag
            bp->tag[btb_idx] = curr_tag;
            if(!bp->isGlobalHist){
                    //local history - initialize to zero
                    *bp->history[btb_idx] = 0;
            }
            if(!bp->isGlobalTable){
                    //local fsm - initialize to default state
                    for(unsigned i = 0 ; i < tableSize ; i++){
                            bp->fsm[btb_idx][i] = bp->fsmState; // initial default state is being set
                    }
            }
    }
    //update the btb row
    update_fsm(&((bp->fsm[btb_idx])[calcTableIndex(pc)]), taken);
    bp->pred_dst[btb_idx] = targetPc;
    *bp->history[btb_idx] = update_history(*bp->history[btb_idx], taken, bp->historySize);
    return;
}

char update_history(char curr_history, bool taken, unsigned historySize){

    unsigned mask = (1 << historySize) - 1; // Create a mask to keep the history within bounds
    
    // Shift the current history left by 1 and add the new taken bit
    return ((curr_history << 1) + (int)taken) & mask;
    // The mask ensures that the history remains within the specified size
    

}
void update_fsm(char *fsm, bool taken){
    // Update the FSM state based on the current state and whether the branch was taken or not
    
    switch (*fsm) {
            case SNT:
                    *fsm = taken ? WNT : SNT;
                    break;
            case WNT:
                    *fsm = taken ? WT : SNT;
                    break;
            case WT:
                    *fsm = taken ? ST : WNT;
                    break;
            case ST:
                    *fsm = taken ? ST : WT;
                    break;
            default:
                    break; // Invalid state
    }

}

void BP_GetStats(SIM_stats *curStats){
    // Stats
    curStats->flush_num = stats.flush_num;           // Machine flushes
	curStats->br_num = stats.br_num;     // Number of branch instructions

	curStats->size = bp->btbSize*(1 + bp->tagSize + ADDRESS_SIZE-2) +
		(bp->isGlobalHist ? 1 : bp->btbSize) * (bp->historySize) +
		(bp->isGlobalTable ? 1 : bp->btbSize) * 2 * (1 << bp->historySize) ;
	
	// Free
	for(int i = 0 ; i < bp->btbSize; i++ ){
	    free(bp->fsm[i]);
	    if(bp->isGlobalTable) {
	        break;
	    }
	}
	free(bp->fsm);
	
	for(int i = 0 ; i < bp->btbSize ; i++ ){
	    free(bp->history[i]);
	    if(bp->isGlobalHist){
	        break;
	    }
	}
	free(bp->history);
	
	free(bp->tag);
	free(bp);
	return;
}
