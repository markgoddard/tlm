/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		10/10/2008
//	File:		TLM.cpp
//
/*********************************************************************************************/

// Header files
#include "stdafx.h"
#include "TLM.h"
#include "TLMMaths.h"
#include "TLMSetup.h"
#include "TLMScene.h"
#include "TLMAlgorithm.h"
#include "TLMOutput.h"
#include "TLMTiming.h"

/* Global variables */

// Algorithm related variables
Node ***Grid;			// The main TLM grid
int xSize = 0;			// The number of nodes in each direction in the grid
int ySize = 0;
int zSize = 0;
TimeVariationSet *TimeVariation;		// For storing time variations of individual nodes
char *InputFilename = "../InputData.txt";

// Input file parameters
char *ProjectName = "TLM";
char *FolderName = "../OutputFiles/Various";
char *SceneFilename = "../SceneFiles/Scene.txt";
char *OutputFilename = "Results.txt";
char *TimeVariationFilename = "TimeVariation.txt";
char *PathLossFilename = "PathLoss.txt";
char *TimingFilename = "Timing.txt";
double GridSpacing = 0.2;
double MaxPathLoss = -160;
double RelativeThreshold = 1E-4;
Source ImpulseSource = {IMPULSE, 0, 0, 0, 1};
double Frequency = 2.4E9;
InputFlags InputData = {{true,true}, {false,true}, {false,true}};
PLParams PathLossParameters = {NONE,0,0,0,0,0,0.2,0.2};
TimingInformation TimingData;


// Input file parameters default flags
bool DefaultProjectName = true;
bool DefaultFolderName = true;
bool DefaultSceneFilename = true;
bool DefaultOutputFilename = true;
bool DefaultTimeVariationFilename = true;
bool DefaultPathLossFilename = true;
bool DefaultTimingFilename = true;
bool DefaultGridSpacing = true;
bool DefaultMaxPathLoss = true;
bool DefaultRelativeThreshold = true;
bool DefaultSourceType = true;
bool DefaultSourceDuration = true;
bool DefaultSourcePosition = true;
bool DefaultFrequency = true;
bool DefaultPLParams = true;


// Main function
int main(int argc, char *argv[])
{
	bool Successful;

	printf("Event Based Scalar TLM Model for Urban Environments\n\n");
	
	if (InputData.PrintTimingInformation.Flag == true) {
		SetStartTime();	
	}	

	// Parse command line input 
	ParseCommandLine(argc,argv);
	
	// Retrieve the setup data
	Successful = ReadDataFromFile();
	
	if (Successful == true) {
		// Display the configuration parameters read from the input file
		DisplayConfigParameters();
		// Perform the relevant actions based on the input file
		Successful = ProcessConfigParameters();
	}

	if (Successful == true) {
		// Read in the scene information from the scene file, and initialise the TLM grid
		if (InputData.PrintTimingInformation.Flag == true) {
			SetSceneParsingStartTime();
		}
		Successful = ReadSceneFile();
		if (InputData.PrintTimingInformation.Flag == true) {
			SetSceneParsingFinishTime();
		}
		PrintImpedances();
	}
	
	// Print the initial grid layout to the display
	if (Successful == true) {

		// Print the timing information
		if (InputData.PrintTimingInformation.Flag == true) {
			SetAlgorithmStartTime();
		}
		// Run the main loop of the TLM algorithm
		MainLoop();

		// Print the timing information
		if (InputData.PrintTimingInformation.Flag == true) {
			SetAlgorithmFinishTime();
		}

		if (InputData.PrintTimeVariation.Flag == true) {
			//Print time variation to output file
			PrintTimeVariation();
			free(TimeVariation);
		}
		
		// Print the path loss values to the path loss file
		if (PathLossParameters.Type != NONE) {
			PrintPathLossToFile();
		}

		if (InputData.PrintTimingInformation.Flag == true) {
			SetFinishTime();
		}

		if (InputData.PrintTimingInformation.Flag == true) {
			PrintTimingInformation();
		}

	//	PrintKappaData();		Used for optimising the path loss constant kappa during testing
	//	printf("%f\n", VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[RoundToNearest(4.5/GridSpacing)][RoundToNearest(3.0/GridSpacing)][RoundToNearest(3.2/GridSpacing)].Vmax * 5.767/4/M_PI/GridSpacing));

		// Deallocate memory for the grid
		free(Grid);

		printf("\nTLM algorithm complete.\n");
	}

	// Wait for user input before exiting
//	printf("Press any key\n");
//	getc(stdin);

	return 0;
}
