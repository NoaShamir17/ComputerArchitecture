/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "./bp_api.h"

#define SUCCESS 0
#define FAILURE -1

struct btb{
	unsigned btbSize;
	unsigned historySize;
	unsigned tagSize;
	unsigned fsmState;
	bool isGlobalHist;
	bool isGlobalTable;
	int Shared;

	unsigned *tag;
	char **history;
	char **fsm;
	uint32_t *pred_dst;
	unsigned flush_num;
}

struct btb *bp;

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){

	*bp = malloc(sizeof(struct btb));
	if (bp == NULL) {
		return FAILURE;
	}
	bp->btbSize = btbSize;
	bp->historySize = historySize;
	bp->tagSize = tagSize;
	bp->fsmState = fsmState;
	bp->isGlobalHist = isGlobalHist;
	bp->isGlobalTable = isGlobalTable;
	bp->Shared = Shared;
	bp->tag = malloc(btbSize * sizeof(unsigned));
	if (bp->tag == NULL) {
		free(bp);
		return FAILURE;
	}
	bp->history = malloc(btbSize * sizeof(char *));
	if (bp->history == NULL) {
		free(bp->tag);
		free(bp);
		return FAILURE;
	}
	bp->fsm = malloc(btbSize * sizeof(char *));
	if (bp->fsm == NULL) {
		free(bp->history);
		free(bp->tag);
		free(bp);
		return FAILURE;
	}
	bp->pred_dst = malloc(btbSize * sizeof(uint32_t));
	if (bp->pred_dst == NULL) {
		free(bp->fsm);
		free(bp->history);
		free(bp->tag);
		free(bp);
		return FAILURE;
	}
	for (unsigned i = 0; i < btbSize; i++) {
		bp->tag[i] = 0;
		bp->history[i] = malloc(historySize * sizeof(char));
		if (bp->history[i] == NULL) {
			for (unsigned j = 0; j < i; j++) {
				free(bp->history[j]);
			}
			free(bp->pred_dst);
			free(bp->fsm);
			free(bp->history);
			free(bp->tag);
			free(bp);
			return FAILURE;
		}
		bp->fsm[i] = malloc(fsmState * sizeof(char));
		if (bp->fsm[i] == NULL) {
			for (unsigned j = 0; j < i; j++) {
				free(bp->history[j]);
				free(bp->fsm[j]);
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

}

bool BP_predict(uint32_t pc, uint32_t *dst){
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

