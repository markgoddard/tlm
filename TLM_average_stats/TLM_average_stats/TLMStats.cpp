/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		6/02/2009
//	File:		TLMStats.cpp
//
/*********************************************************************************************/

#include "stdafx.h"
#include "TLMStats.h"

extern Stats *StatsList;


Stats *NewStats(void)
{
	Stats *NewStats = (Stats*)malloc(sizeof(Stats));
	NewStats->NextStats = NULL;

	return NewStats;
}


void FreeStatsList(void)
{
	Stats *CurrentStats = StatsList;
	Stats *NextStats;

	while (CurrentStats != NULL) {
		NextStats = CurrentStats->NextStats;
		free(CurrentStats);
		CurrentStats = NextStats;
	}
}