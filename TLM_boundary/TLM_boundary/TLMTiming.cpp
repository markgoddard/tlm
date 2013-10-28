/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		30/11/2008
//	File:		TLMTiming.h
//
/*********************************************************************************************/

#include "stdafx.h"
#include "TLM.h"
#include "TLMTiming.h"

// Global variables
extern char *ProjectName;
extern char *FolderName;


// Timing related variables
extern TimingInformation TimingData;

void SetStartTime(void)
{
	TimingData.StartTime = clock();
}


void SetSceneParsingStartTime(void)
{
	TimingData.SceneParsingStartTime = clock();
}


void SetSceneParsingFinishTime(void)
{
	TimingData.SceneParsingFinishTime = clock();
}


void SetAlgorithmStartTime(void)
{
	TimingData.AlgorithmStartTime = clock();
}


void SetAlgorithmFinishTime(void)
{
	TimingData.AlgorithmFinishTime = clock();
}


void SetFinishTime(void)
{
	TimingData.FinishTime = clock();
}