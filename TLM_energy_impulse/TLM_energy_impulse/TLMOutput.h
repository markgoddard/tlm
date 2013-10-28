/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		24/11/2008
//	File:		TLMOutput.h
//
/*********************************************************************************************/

#ifndef TLM_OUTPUT_H
#define TLM_OUTPUT_H

// Function prototypes
void PrintPathLossToFile(void);
void PrintImpedances(void);
void SaveTimeVariation(TimeVariationSet *CurrentSet);
void PrintTimeVariation(void);
void PrintKappaData(void);
void PrintTimingInformation(void);

#endif //TLM_OUTPUT_H