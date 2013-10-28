/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		20/10/2008
//	File:		TLMSetup.h
//
/*********************************************************************************************/

#ifndef TLM_SETUP_H
#define TLM_SETUP_H

// Function prototypes
void ParseCommandLine(int argc, char *argv[]);
bool ReadDataFromFile(void);
void DisplayConfigParameters(void);
bool ProcessConfigParameters(void);

#endif //TLM_SETUP_H