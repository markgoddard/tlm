/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		24/11/2008
//	File:		TLMOutput.c
//
/*********************************************************************************************/

#include "stdafx.h"
#include "TLM.h"
#include "TLMMaths.h"
#include "TLMTiming.h"
#include "TLMScene.h"
#include "TLMStats.h"


// Definitions
// The path loss constant
//#define KAPPA 5.7672
//#define KAPPA 3.87
#define KAPPA 6.8505


/* Global variables */

// Algorithm related variables
extern Node ***Grid;			// The main TLM grid
extern int xSize;			// The number of nodes in each direction in the grid
extern int ySize;
extern int zSize;			// Maximum number of iterations to be completed
extern TimeVariationSet *TimeVariation;		// For storing time variations of individual nodes
extern TimingInformation TimingData;
extern Stats *StatsList;

// Input file parameters
extern char *FolderName;
extern char *ProjectName;
extern char *OutputFilename;
extern char *SceneFilename;
extern char *PathLossFilename;
extern char *TimeVariationFilename;
extern char *TimingFilename;
extern char *StatsFilename;
extern double GridSpacing;
extern double Frequency;
extern Source ImpulseSource;
extern PLParams PathLossParameters;


// Print the project name and a time and date stamp to the top of a file 
void PrintFileHeader(FILE *File)
{
	char *TimeBuffer;
	struct tm Time;
	time_t PrintTime;
	TimeBuffer = (char*)malloc(100*sizeof(char));

	time(&PrintTime);
	gmtime_s(&Time, &PrintTime);
	asctime_s(TimeBuffer, 100, &Time);

	fprintf(File,"Event based TLM\nProject: %s\n%s\n\n", ProjectName, TimeBuffer);

	free(TimeBuffer);
}


// Print the estimated path loss to a text file. Either print a point, route (line) or grid of estimates
void PrintPathLossToFile(void)
{	
	FILE *PathLossFile;
	char *FilenameBuffer;

	FilenameBuffer = (char*)malloc(100*sizeof(char));

	sprintf_s(FilenameBuffer, 100*sizeof(char), "%s/%s_%s", FolderName, ProjectName, PathLossFilename);

	if (fopen_s(&PathLossFile, FilenameBuffer, "w") != 0) {
		printf("Could not open file '%s'\n", PathLossFilename);
	}
	else {

		printf("Printing path loss values to '%s'\n",PathLossFilename);
		PrintFileHeader(PathLossFile);

		int x, y, z;
		double PathLoss;
		

		// Z coordinate is constant
		z = PlaceWithinGridZ(RoundToNearest(PathLossParameters.Z1/GridSpacing));
		
		switch (PathLossParameters.Type) {
			// Print the path loss at a single point
			case POINT: {
				// Details of the path loss estimates required and column titles
				fprintf(PathLossFile, "Point Analysis\nHeight = %f\n\nX\t\tY\t\tPL(dB)\n", z*GridSpacing);
				x = PlaceWithinGridX(RoundToNearest(PathLossParameters.X1/GridSpacing));
				y = PlaceWithinGridY(RoundToNearest(PathLossParameters.Y1/GridSpacing));
				PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
				fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
				break;
			}
			// Print the path loss along a route
			case ROUTE: {
				double length = sqrt(SQUARE(PathLossParameters.X2 - PathLossParameters.X1)+SQUARE(PathLossParameters.Y2 - PathLossParameters.Y1));
				double nSamples = length/PathLossParameters.Spacing;
				double dx = (PathLossParameters.X2 - PathLossParameters.X1)/nSamples;
				double dy = (PathLossParameters.Y2 - PathLossParameters.Y1)/nSamples;

				// Details of the path loss estimates required and column titles
				fprintf(PathLossFile, "Route Analysis - %d samples\nHeight = %f\n\nX\t\tY\t\tPL(dB)\n", (int)nSamples == nSamples ? (int)nSamples+1 : (int)nSamples+2, z*GridSpacing);

				for (int i = 0; i < nSamples; i++) {
					x = PlaceWithinGridX(RoundToNearest((PathLossParameters.X1+dx*i)/GridSpacing));
					y = PlaceWithinGridY(RoundToNearest((PathLossParameters.Y1+dy*i)/GridSpacing));
					PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
					fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
				}
				x = PlaceWithinGridX(RoundToNearest(PathLossParameters.X2/GridSpacing));
				y = PlaceWithinGridY(RoundToNearest(PathLossParameters.Y2/GridSpacing));
				PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
				fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
				break;
			}

			// Print the path loss in a 2d grid
			case GRID: {
				double nSamplesX = (PathLossParameters.X2-PathLossParameters.X1)/PathLossParameters.Spacing;
				double nSamplesY = (PathLossParameters.Y2-PathLossParameters.Y1)/PathLossParameters.SpacingY;
				double dx = (PathLossParameters.X2 - PathLossParameters.X1)/nSamplesX;
				double dy = (PathLossParameters.Y2 - PathLossParameters.Y1)/nSamplesY;

				// Details of the path loss estimates required and column titles
				fprintf(PathLossFile, "Grid Analysis - %d x %d samples\nHeight = %f\n\nX\t\tY\t\tPL(dB)\n", (int)nSamplesX == nSamplesX ? (int)nSamplesX+1 : (int)nSamplesX+2, (int)nSamplesY == nSamplesY ? (int)nSamplesY+1 : (int)nSamplesY+2, z*GridSpacing);
				
				for (int j=0; j < nSamplesY; j++) {
					for (int i=0; i < nSamplesX; i++) {
						x = PlaceWithinGridX(RoundToNearest((PathLossParameters.X1+dx*i)/GridSpacing));
						y = PlaceWithinGridY(RoundToNearest((PathLossParameters.Y1+dy*j)/GridSpacing));
						PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
						fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
					}
					x = PlaceWithinGridX(RoundToNearest(PathLossParameters.X2/GridSpacing));
					y = PlaceWithinGridY(RoundToNearest((PathLossParameters.Y1+dy*j)/GridSpacing));
					PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
					fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
				}
				// Print a path loss estimate of the outer X row nearest X2,Y2 on the grid (may not be a whole sample space apart)
				for (int i=0; i < nSamplesX; i++) {
					x = PlaceWithinGridX(RoundToNearest((PathLossParameters.X1+dx*i)/GridSpacing));
					y = PlaceWithinGridY(RoundToNearest(PathLossParameters.Y2/GridSpacing));
					PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
					fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
				}
				x = PlaceWithinGridX(RoundToNearest(PathLossParameters.X2/GridSpacing));
				y = PlaceWithinGridY(RoundToNearest(PathLossParameters.Y2/GridSpacing));
				PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
				fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
				break;
			}

			// Print the path loss in a set of 2d grids
			case CUBE: {
				double nSamplesX = (PathLossParameters.X2-PathLossParameters.X1)/PathLossParameters.Spacing;
				double nSamplesY = (PathLossParameters.Y2-PathLossParameters.Y1)/PathLossParameters.SpacingY;
				double nSamplesZ = (PathLossParameters.Z2-PathLossParameters.Z1)/PathLossParameters.SpacingZ;
				double dx = (PathLossParameters.X2 - PathLossParameters.X1)/nSamplesX;
				double dy = (PathLossParameters.Y2 - PathLossParameters.Y1)/nSamplesY;
				double dz = (PathLossParameters.Z2 - PathLossParameters.Z1)/nSamplesZ;

				// Details of the path loss estimates required and column titles
				fprintf(PathLossFile, "Grid Analysis - %d x %d x %d samples\n", (int)nSamplesX == nSamplesX ? (int)nSamplesX+1 : (int)nSamplesX+2, (int)nSamplesY == nSamplesY ? (int)nSamplesY+1 : (int)nSamplesY+2, (int)nSamplesZ == nSamplesZ ? (int)nSamplesZ+1 : (int)nSamplesZ+2);
				
				for (int k=0; k < nSamplesZ; k++) {
					z = PlaceWithinGridZ(RoundToNearest((PathLossParameters.Z1+dz*k)/GridSpacing));
					fprintf(PathLossFile, "\nHeight = %f\n\nX\t\tY\t\tPL(dB)\n", z*GridSpacing);
				
					for (int j=0; j < nSamplesY; j++) {
						for (int i=0; i < nSamplesX; i++) {
							x = PlaceWithinGridX(RoundToNearest((PathLossParameters.X1+dx*i)/GridSpacing));
							y = PlaceWithinGridY(RoundToNearest((PathLossParameters.Y1+dy*j)/GridSpacing));
							PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
							fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
						}
						x = PlaceWithinGridX(RoundToNearest(PathLossParameters.X2/GridSpacing));
						y = PlaceWithinGridY(RoundToNearest((PathLossParameters.Y1+dy*j)/GridSpacing));
						PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
						fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
					}
					// Print a path loss estimate of the outer X row nearest X2,Y2 on the grid (may not be a whole sample space apart)
					for (int i=0; i < nSamplesX; i++) {
						x = PlaceWithinGridX(RoundToNearest((PathLossParameters.X1+dx*i)/GridSpacing));
						y = PlaceWithinGridY(RoundToNearest(PathLossParameters.Y2/GridSpacing));
						PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
						fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
					}
					x = PlaceWithinGridX(RoundToNearest(PathLossParameters.X2/GridSpacing));
					y = PlaceWithinGridY(RoundToNearest(PathLossParameters.Y2/GridSpacing));
					PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
					fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
				}

				z = PlaceWithinGridZ(RoundToNearest((PathLossParameters.Z2)/GridSpacing));
				fprintf(PathLossFile, "\nHeight = %f\n\nX\t\tY\t\tPL(dB)\n", z*GridSpacing);
				
				for (int j=0; j < nSamplesY; j++) {
					for (int i=0; i < nSamplesX; i++) {
						x = PlaceWithinGridX(RoundToNearest((PathLossParameters.X1+dx*i)/GridSpacing));
						y = PlaceWithinGridY(RoundToNearest((PathLossParameters.Y1+dy*j)/GridSpacing));
						PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
						fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
					}
					x = PlaceWithinGridX(RoundToNearest(PathLossParameters.X2/GridSpacing));
					y = PlaceWithinGridY(RoundToNearest((PathLossParameters.Y1+dy*j)/GridSpacing));
					PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
					fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
				}
				// Print a path loss estimate of the outer X row nearest X2,Y2 on the grid (may not be a whole sample space apart)
				for (int i=0; i < nSamplesX; i++) {
					x = PlaceWithinGridX(RoundToNearest((PathLossParameters.X1+dx*i)/GridSpacing));
					y = PlaceWithinGridY(RoundToNearest(PathLossParameters.Y2/GridSpacing));
					PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
					fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
				}
				x = PlaceWithinGridX(RoundToNearest(PathLossParameters.X2/GridSpacing));
				y = PlaceWithinGridY(RoundToNearest(PathLossParameters.Y2/GridSpacing));
				PathLoss = VoltageToDB(SPEED_OF_LIGHT/Frequency * Grid[x][y][z].Vmax * KAPPA/4/M_PI/GridSpacing);
				fprintf(PathLossFile, "%f\t%f\t%f\n", x*GridSpacing, y*GridSpacing, PathLoss);
				break;
			}
		}

		if (fclose(PathLossFile)) {
			printf("Path loss file close unsuccessful\n");
		}
	}
	free(FilenameBuffer);	
}


// Print statistics to a file
void PrintStatsToFile()
{	
	FILE *StatsFile;
	char *FilenameBuffer;

	if (StatsList != NULL) {

		FilenameBuffer = (char*)malloc(100*sizeof(char));

		sprintf_s(FilenameBuffer, 100*sizeof(char), "%s/%s_%s", FolderName, ProjectName, StatsFilename);

		if (fopen_s(&StatsFile, FilenameBuffer, "w") != 0) {
			printf("Could not open file '%s'\n", StatsFilename);
		}
		else {
			Stats *CurrentStats = StatsList;
			int n = 0;

			printf("Printing statistics to '%s'\n",StatsFilename);
			PrintFileHeader(StatsFile);

			fprintf(StatsFile,"\nIteration\tActive Nodes\tAdded Nodes\tRemoved Both\tRemoved Abs\tRemoved Rel\tTotal Calcs\tBoundary Calcs\tNew Maximums\tIteration Time\n");

			while (CurrentStats != NULL) {
				fprintf(StatsFile, "%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\n", n, CurrentStats->nActiveNodes, CurrentStats->nAddedNodes, CurrentStats->nRemovedBoth, CurrentStats->nRemovedAbsolute, CurrentStats->nRemovedRelative, CurrentStats->nCalcs, CurrentStats->nBoundaryCalcs, CurrentStats->nNewMaximums, CurrentStats->IterationTime);

				CurrentStats = CurrentStats->NextStats;
				n++;
			}
	
			if (fclose(StatsFile)) {
				printf("Path loss file close unsuccessful\n");
			}
		}
		free(FilenameBuffer);	
	}
	else {
		printf("No statistics to print");
	}
}


// Print the impedances of nodes to a text file
void PrintImpedances(void)
{
	FILE *OutputFile;
	double Impedance;
	char *FilenameBuffer;

	FilenameBuffer = (char*)malloc(100*sizeof(char));

	sprintf_s(FilenameBuffer, 100*sizeof(char), "%s/%s_%s", FolderName, ProjectName, OutputFilename);
	
	if (fopen_s(&OutputFile, FilenameBuffer, "w") != 0) {
		printf("Could not open file '%s'\n", OutputFilename);
	}
	else {
		printf("Printing Impedances to output file '%s'\n", OutputFilename);
		PrintFileHeader(OutputFile);
		for (int y = ySize-1; y>=0; y--) {
			for (int x = 0; x < xSize; x++) {
				Impedance = Grid[x][y][zSize/2].Z;
				if (x == ImpulseSource.X && y == ImpulseSource.Y) {
					fputc('i',OutputFile);
				}
				else if (x==PlaceWithinGridX(RoundToNearest(PathLossParameters.X1/GridSpacing)) && y == PlaceWithinGridY(RoundToNearest(PathLossParameters.Y1/GridSpacing))) {
					fputc('1',OutputFile);
				}
				else if (PathLossParameters.Type != POINT && x==PlaceWithinGridX(RoundToNearest(PathLossParameters.X2/GridSpacing)) && y == PlaceWithinGridY(RoundToNearest(PathLossParameters.Y2/GridSpacing))) {
					fputc('2',OutputFile);
				}
				// E = 1
				else if (Impedance == IMPEDANCE_OF_FREE_SPACE) {
					fputc('.',OutputFile);
				}
				// E <= 2
				else if (Impedance >= IMPEDANCE_OF_FREE_SPACE/sqrt(2.0)) {
					fputc('W', OutputFile);
				}
				// E <= 5
				else if (Impedance >= IMPEDANCE_OF_FREE_SPACE/sqrt(5.0)) {
					fputc('#', OutputFile);
				}
				// E >= 5
				else {
					fputc(' ', OutputFile);
				}
			}
			fprintf(OutputFile, "\n");
		}

		if (fclose(OutputFile)) {
			printf("Output file close unsuccessful\n");
		}
	}
	free(FilenameBuffer);
}



// Save the variation of a row of nodes over time
void SaveTimeVariation(TimeVariationSet *CurrentSet)
{
	CurrentSet->V = (double*)malloc(xSize*sizeof(double));
	for (int x = 0; x < xSize; x++) {
		CurrentSet->V[x] = Grid[x][ySize/2][zSize/2].V;
	}
}

// Print the variation of a row of nodes over time
void PrintTimeVariation(void)
{
	FILE *OutputFile;
	TimeVariationSet *CurrentTimeVariation;
	char *FilenameBuffer;

	FilenameBuffer = (char*)malloc(100*sizeof(char));

	sprintf_s(FilenameBuffer, 100*sizeof(char), "%s/%s_%s", FolderName, ProjectName, TimeVariationFilename);
	
	if (fopen_s(&OutputFile, FilenameBuffer, "w") != 0) {
		printf("Could not open file '%s'\n", TimeVariationFilename);
	}
	else {
		printf("Printing time variation to output file %s\n", TimeVariationFilename);
		PrintFileHeader(OutputFile);
		CurrentTimeVariation = TimeVariation;
		while (CurrentTimeVariation != NULL) {
			for (int x = 0; x < xSize; x++) {
				fprintf(OutputFile, "%f\t", CurrentTimeVariation->V[x]);
			}
			fprintf(OutputFile, "\n");
			CurrentTimeVariation = CurrentTimeVariation->NextSet;
		}

		if (fclose(OutputFile)) {
			printf("Output file close unsuccessful\n");
		}
	}
	free(FilenameBuffer);
}

// Print information required to estimate the path loss error constant kappa to a text file
void PrintKappaData(void) 
{
	FILE *OutputFile;
	char *FilenameBuffer;

	FilenameBuffer = (char*)malloc(100*sizeof(char));

	sprintf_s(FilenameBuffer, 100*sizeof(char), "%s/%s_%s", FolderName, ProjectName, "KappaCalcs.txt");

	int x, y, z;
	double d;
	
	if (fopen_s(&OutputFile, FilenameBuffer, "w") != 0) {
		printf("Could not open file '%s'\n", "KappaCalcs.txt");
	}
	else {
		printf("Printing time variation to output file %s\n", "KappaCalcs.txt");
		PrintFileHeader(OutputFile);
		
		fprintf(OutputFile,"V\tDistance\tTheta\tPhi\n");
		for (int xx = 0; xx < 10; xx++) { 
			for (int yy = 0; yy < 10; yy++) {
				for (int zz = 0; zz < 10; zz++) {
					x = RoundToNearest(ImpulseSource.X + 9*xx*GridSpacing);
					y = RoundToNearest(ImpulseSource.Y + 9*yy*GridSpacing);
					z = RoundToNearest(ImpulseSource.Z + 9*zz*GridSpacing);
					d = 9*sqrt((double)(SQUARE(xx)+SQUARE(yy)+SQUARE(zz)));
					fprintf(OutputFile,"%f\t%f\t%f\t%f\n", Grid[x][y][z].Vmax, d, asin((9.0*zz)/d), atan((double)(yy)/xx));
				}
			}
		}


		if (fclose(OutputFile)) {
			printf("Output file close unsuccessful\n");
		}
	}
	free(FilenameBuffer);
}


// Print information on the time taken to execute the program
void PrintTimingInformation(void)
{
	FILE *TimingFile;
	char *FilenameBuffer;

	FilenameBuffer = (char*)malloc(100*sizeof(char));

	sprintf_s(FilenameBuffer, 100*sizeof(char), "%s/%s_%s", FolderName, ProjectName, TimingFilename);
	
	if (fopen_s(&TimingFile, FilenameBuffer, "w") != 0) {
		printf("Could not open file '%s'\n", TimingFilename);
	}
	else {
		printf("Printing timing information to '%s'\n", TimingFilename);
		PrintFileHeader(TimingFile);
		fprintf(TimingFile, "Timing information for scene file '%s'\n", SceneFilename);
		fprintf(TimingFile, "TotalTime = %dms\nScene parsing time = %dms\nAlgorithm time = %dms\n", TimingData.FinishTime - TimingData.StartTime, TimingData.SceneParsingFinishTime - TimingData.SceneParsingStartTime, TimingData.AlgorithmFinishTime - TimingData.AlgorithmStartTime);
	}
	free(FilenameBuffer);
}