/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		6/02/2009
//	File:		TLMSats.h
//
/*********************************************************************************************/

#ifndef TLM_STATS_H
#define TLM_STATS_H

// Structure to hold statistics for a single iteration
typedef struct STATS Stats;

struct STATS {	int nActiveNodes;
				int nAddedNodes;
				int nRemovedBoth;
				int nRemovedAbsolute;
				int nRemovedRelative;
				int nCalcs;
				int nBoundaryCalcs;
				int nNewMaximums;
				int IterationTime;
				Stats *NextStats;
};

// Function Prototypes
Stats *NewStats(void);
void FreeStatsList(void);

#endif //TLM_STATS_H