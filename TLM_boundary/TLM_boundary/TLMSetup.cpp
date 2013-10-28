/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		20/10/2008
//	File:		TLMSetup.cpp
//
/*********************************************************************************************/

// Header files
#include "stdafx.h"
#include "TLMSetup.h"
#include "TLMMaths.h"
#include "TLM.h"


// Function prototypes
bool GetFieldFromFile(FILE *File, const char *FieldName, char **FieldValue);

// Reference Global variables
extern Node ***Grid;

// Input file parameters
extern char *ProjectName;
extern char *FolderName;
extern char *InputFilename;
extern char *SceneFilename;
extern char *OutputFilename;
extern char *TimeVariationFilename;
extern char *PathLossFilename;
extern char *TimingFilename;
extern double GridSpacing;
extern double MaxPathLoss;
extern double RelativeThreshold;
extern Source ImpulseSource;
extern double Frequency;
extern InputFlags InputData;
extern PLParams PathLossParameters;

// Input file parameters default flags
extern bool DefaultProjectName;
extern bool DefaultFolderName;
extern bool DefaultSceneFilename;
extern bool DefaultOutputFilename;
extern bool DefaultTimeVariationFilename;
extern bool DefaultPathLossFilename;
extern bool DefaultTimingFilename;
extern bool DefaultGridSpacing;
extern bool DefaultMaxPathLoss;
extern bool DefaultRelativeThreshold;
extern bool DefaultSourceType;
extern bool DefaultSourceDuration;
extern bool DefaultSourcePosition;
extern bool DefaultFrequency;
extern bool DefaultPLParams;


// Function prototypes
bool ReadString(char **Context, char **String, bool *DefaultFlag);
bool ReadDouble(char **Context, double *Double, bool *DefaultFlag);
bool ReadInt(char **Context, int *Int, bool *DefaultFlag);
bool ReadBool(char **Context, bool *Bool, bool *DefaultFlag);
void DisplayParameter(const char *ParameterName, char *ParameterValue, bool DefaultFlag);


// Parse command line arguments
void ParseCommandLine(int argc, char *argv[])
{
	if (argc == 2) {
		InputFilename = _strdup(argv[1]);
		printf("Input file '%s' specified at command line",InputFilename);
	}
}

// Read input data from a text file
bool ReadDataFromFile(void)
{
	FILE *InputFile;
	bool ReturnValue = true;

	if (fopen_s(&InputFile, InputFilename, "r") != 0) {
		printf("Could not open input file '%s'\n", InputFilename);
		ReturnValue = false;
	}
	else {
		// Flags used in reading input file data
		bool EndOfFile;
		bool SuccessfulRead;
		// Variables used in file reading
		char *Buffer;
		char *ParameterName;
		char *Comment;
		char *Context = NULL;
		int LineCount;
		double MaxPathLoss = 0;

		// Allocate memory for the buffer
		Buffer = (char*) malloc(200*sizeof(char));

		// Initialise the flags
		EndOfFile = false;
		SuccessfulRead = true;

		// Initialise the line count to 0
		LineCount = 0;

		printf("\nReading configuration information from input file '%s'\n\n", InputFilename);

		// Read the input file, one line at a time
		do {
			if (fgets(Buffer, 200, InputFile) != NULL) {
				
				// Keep track of the line number
				LineCount++;
				// Search for and remove comments				
				Comment = strstr(Buffer, "//");
				if (Comment != NULL) {
					Comment[0] = NULL;	// Replace with 0 to mark a new end of the string
				}

				// Remove leading whitespace and read the parameter name
				ParameterName = strtok_s(Buffer, " \t", &Context);
				if (ParameterName != NULL) {
					// Read the project name
					if (strcmp(ParameterName, "project_name") == 0) {
						if (ReadString(&Context, &ProjectName, &DefaultProjectName) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the project folder name
					else if (strcmp(ParameterName, "folder_name") == 0) {
						if (ReadString(&Context, &FolderName, &DefaultFolderName) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the scene file name
					else if (strcmp(ParameterName, "scene_filename") == 0) {
						if (ReadString(&Context, &SceneFilename, &DefaultSceneFilename) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the output file name
					else if (strcmp(ParameterName, "output_filename") == 0) {
						if (ReadString(&Context, &OutputFilename, &DefaultOutputFilename) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the time variation file name
					else if (strcmp(ParameterName, "time_variation_filename") == 0) {
						if (ReadString(&Context, &TimeVariationFilename, &DefaultTimeVariationFilename) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss file name
					else if (strcmp(ParameterName, "path_loss_filename") == 0) {
						if (ReadString(&Context, &PathLossFilename, &DefaultPathLossFilename) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the timing file name
					else if (strcmp(ParameterName, "timing_filename") == 0) {
						if (ReadString(&Context, &TimingFilename, &DefaultTimingFilename) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the grid spacing
					else if (strcmp(ParameterName, "grid_spacing") == 0) {
						if (ReadDouble(&Context, &GridSpacing, &DefaultGridSpacing) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the input source type
					else if (strcmp(ParameterName, "source_type") == 0) {
						char *SourceTypeString = NULL;

						if (ReadString(&Context, &SourceTypeString, &DefaultSourceType) == false) {
							SuccessfulRead = false;
						}
						else {
							if (strcmp(SourceTypeString, "impulse") == 0) {
								ImpulseSource.Type = IMPULSE;
							}
							else if (strcmp(SourceTypeString, "gaussian") == 0) {
								ImpulseSource.Type = GAUSSIAN;
							}
							else if (strcmp(SourceTypeString, "raised_cosine") == 0) {
								ImpulseSource.Type = RAISED_COSINE;
							}
							else {
								SuccessfulRead = false;
							}
						}
					}
					// Read the source duration
					else if (strcmp(ParameterName, "source_duration") == 0) {
						if (ReadInt(&Context, &ImpulseSource.Duration, &DefaultSourceDuration) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the source position x
					else if (strcmp(ParameterName, "source_position_x") == 0) {
						double Position = NULL;

						if (ReadDouble(&Context, &Position, &DefaultSourcePosition) == false) {
							SuccessfulRead = false;
						}
						else {
							ImpulseSource.X = RoundToNearest(Position/GridSpacing);
						}
					}
					// Read the source position y
					else if (strcmp(ParameterName, "source_position_y") == 0) {
						double Position = NULL;

						if (ReadDouble(&Context, &Position, &DefaultSourcePosition) == false) {
							SuccessfulRead = false;
						}
						else {
							ImpulseSource.Y = RoundToNearest(Position/GridSpacing);
						}
					}
					// Read the source position z
					else if (strcmp(ParameterName, "source_position_z") == 0) {
						double Position = NULL;

						if (ReadDouble(&Context, &Position, &DefaultSourcePosition) == false) {
							SuccessfulRead = false;
						}
						else {
							ImpulseSource.Z = RoundToNearest(Position/GridSpacing);
						}
					}
					// Read the operating frequency
					else if (strcmp(ParameterName, "frequency") == 0) {
						if (ReadDouble(&Context, &Frequency, &DefaultFrequency) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss threshold
					else if (strcmp(ParameterName, "max_path_loss") == 0) {
						if (ReadDouble(&Context, &MaxPathLoss, &DefaultMaxPathLoss) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the relative voltage threshold
					else if (strcmp(ParameterName, "relative_threshold") == 0) {
						if (ReadDouble(&Context, &RelativeThreshold, &DefaultRelativeThreshold) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the display polygons flag
					else if (strcmp(ParameterName, "display_polygons") == 0) {
						if (ReadBool(&Context, &InputData.DisplayPolygonInformation.Flag, &InputData.DisplayPolygonInformation.Default) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the print time variation flag
					else if (strcmp(ParameterName, "print_time_variation") == 0) {
						if (ReadBool(&Context, &InputData.PrintTimeVariation.Flag, &InputData.PrintTimeVariation.Default) == false) {
							SuccessfulRead = false;
						}
					}

					// Read the output path loss flag
					else if (strcmp(ParameterName, "output_path_loss") == 0) {
						char *PathLossString = NULL;

						if (ReadString(&Context, &PathLossString, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
						else {
							if (strcmp(PathLossString, "none") == 0) {
								PathLossParameters.Type = NONE;
							}
							else if (strcmp(PathLossString, "point") == 0) {
								PathLossParameters.Type = POINT;
							}
							else if (strcmp(PathLossString, "route") == 0) {
								PathLossParameters.Type = ROUTE;
							}
							else if (strcmp(PathLossString, "grid") == 0) {
								PathLossParameters.Type = GRID;
							}
							else if (strcmp(PathLossString, "cube") == 0) {
								PathLossParameters.Type = CUBE;
							}
							else {
								SuccessfulRead = false;
							}
						}
					}
					// Read the path loss x1 coordinate
					else if (strcmp(ParameterName, "pl_x1") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.X1, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss y1 coordinate
					else if (strcmp(ParameterName, "pl_y1") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.Y1, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss x2 coordinate
					else if (strcmp(ParameterName, "pl_x2") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.X2, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss y2 coordinate
					else if (strcmp(ParameterName, "pl_y2") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.Y2, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss height coordinate
					else if (strcmp(ParameterName, "pl_z") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.Z1, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss height coordinate 1 (cube)
					else if (strcmp(ParameterName, "pl_z1") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.Z1, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss height coordinate 2 (cube)
					else if (strcmp(ParameterName, "pl_z2") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.Z2, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss spacing
					else if (strcmp(ParameterName, "pl_spacing") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.Spacing, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss x spacing
					else if (strcmp(ParameterName, "pl_spacing_x") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.Spacing, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss y spacing
					else if (strcmp(ParameterName, "pl_spacing_y") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.SpacingY, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the path loss z spacing
					else if (strcmp(ParameterName, "pl_spacing_z") == 0) {
						if (ReadDouble(&Context, &PathLossParameters.SpacingZ, &DefaultPLParams) == false) {
							SuccessfulRead = false;
						}
					}
					// Read the timing flag
					else if (strcmp(ParameterName, "store_timing") == 0) {
						if (ReadBool(&Context, &InputData.PrintTimingInformation.Flag, &InputData.PrintTimingInformation.Default) == false) {
							SuccessfulRead = false;
						}
					}
				}
			}
			else if (feof(InputFile) != 0) {
				EndOfFile = true;
			}
		}
		while (EndOfFile == false && SuccessfulRead == true);
				
		// Free allocated memory
		free(Buffer);
		fclose(InputFile);

		// Check to see if there were any errors in the scene file
		if (SuccessfulRead == false) {
			printf("Error in input file, line %d\n", LineCount);
			return false;
		}
		else {
			printf("Input file parsed successfully\n\n");
		}
	}

	return true;
}


// Read a string from the input file
bool ReadString(char **Context, char **String, bool *DefaultFlag)
{
	char *Parameter;

	if (*Context[0] == '=') {
		Parameter = strtok_s(NULL, "\t\n =", Context);
		if (Parameter != NULL) {
			*String = _strdup(Parameter);
			*DefaultFlag = false;
			return true;
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}


// Read a floating point number from the input file
bool ReadDouble(char **Context, double *Double, bool *DefaultFlag)
{	
	char *Parameter;

	if (*Context[0] == '=') {
		Parameter = strtok_s(NULL, "\t\n =", Context);
		if (Parameter != NULL) {
			*Double = atof(Parameter);
			*DefaultFlag = false;
			return true;
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}


// Read an integer from the input file
bool ReadInt(char **Context, int *Int, bool *DefaultFlag)
{	
	char *Parameter;

	if (*Context[0] == '=') {
		Parameter = strtok_s(NULL, "\t\n =", Context);
		if (Parameter != NULL) {
			*Int = atoi(Parameter);
			*DefaultFlag = false;
			return true;
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}

// Read a boolean flag from the input file
bool ReadBool(char **Context, bool *Bool, bool *DefaultFlag)
{	
	char *Parameter;

	if (*Context[0] == '=') {
		Parameter = strtok_s(NULL, "\t\n =", Context);
		if (Parameter != NULL) {
			if (strcmp(Parameter, "true") == 0) {
				*Bool = true;
			}
			else if (strcmp(Parameter, "false") == 0) {
				*Bool = false;
			}
			else {
				return false;
		}
			*DefaultFlag = false;
			return true;
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}


// Display the configuration parameters 
void DisplayConfigParameters(void)
{
	char *Buffer;
	size_t BufferSize;

	// Allocate memory for the buffer
	BufferSize = 50*sizeof(char);
	Buffer = (char*)malloc(BufferSize);

	// Display the project name
	DisplayParameter("Project name", ProjectName, DefaultProjectName);
	
	// Display the folder name
	DisplayParameter("Folder name", FolderName, DefaultFolderName);
	
	// Display the scene filename
	DisplayParameter("Scene filename", SceneFilename, DefaultSceneFilename);
	
	// Display the output filename
	DisplayParameter("Output filename", OutputFilename, DefaultOutputFilename);
	
	// Display the grid spacing
	sprintf_s(Buffer, BufferSize, "%.2e", GridSpacing); 
	DisplayParameter("Grid spacing", Buffer, DefaultGridSpacing);
	
	// Display the source type
	switch (ImpulseSource.Type) {
		case IMPULSE:
			sprintf_s(Buffer, BufferSize, "impulse");
			break;
		case GAUSSIAN:
			sprintf_s(Buffer, BufferSize, "gaussian");
			break;
		case RAISED_COSINE:
			sprintf_s(Buffer, BufferSize, "raised cosine");
			break;
	}
	DisplayParameter("Source type", Buffer, DefaultSourceType);
	
	// Display the source duration
	sprintf_s(Buffer, BufferSize, "%d", ImpulseSource.Duration);
	DisplayParameter("Source duration", Buffer, DefaultSourceDuration);
	
	// Display the source position x
	sprintf_s(Buffer, BufferSize, "%f", ImpulseSource.X*GridSpacing);
	DisplayParameter("Source position x", Buffer, DefaultSourcePosition);
	
	// Display the source position y
	sprintf_s(Buffer, BufferSize, "%f", ImpulseSource.Y*GridSpacing);
	DisplayParameter("Source position y", Buffer, DefaultSourcePosition);
	
	// Display the source position z
	sprintf_s(Buffer, BufferSize, "%f", ImpulseSource.Z*GridSpacing);
	DisplayParameter("Source position z", Buffer, DefaultSourcePosition);
	
	// Display the operating frequency
	sprintf_s(Buffer, BufferSize, "%.2eHz", Frequency);
	DisplayParameter("Operating frequency", Buffer, DefaultFrequency);
	
	// Display the absolute voltage threshold
	sprintf_s(Buffer, BufferSize, "%.2f", MaxPathLoss);
	DisplayParameter("Maximum path loss", Buffer, DefaultMaxPathLoss);
	
	// Display the relative voltage threshold
	sprintf_s(Buffer, BufferSize, "%.2e", RelativeThreshold);
	DisplayParameter("Relative threshold", Buffer, DefaultRelativeThreshold);
	
	// Display the display polygons flag
	DisplayParameter("Display polygons", InputData.DisplayPolygonInformation.Flag == true ? "true" : "false",InputData.DisplayPolygonInformation.Default);
	
	// Display the print time variation flag
	DisplayParameter("Print time variation", InputData.PrintTimeVariation.Flag == true ? "true" : "false", InputData.PrintTimeVariation.Default);
	if (InputData.PrintTimeVariation.Flag == true) {
		// Display the output filename
		DisplayParameter("Time variation filename", TimeVariationFilename, DefaultTimeVariationFilename);
	}
	
	// Display the output path loss type
	switch (PathLossParameters.Type) {
		case NONE:
			sprintf_s(Buffer, BufferSize, "none");
			break;
		case POINT:
			sprintf_s(Buffer, BufferSize, "point");
			break;
		case ROUTE:
			sprintf_s(Buffer, BufferSize, "route");
			break;
		case GRID:
			sprintf_s(Buffer, BufferSize, "grid");
			break;
		case CUBE:
			sprintf_s(Buffer, BufferSize, "cube");
			break;
		default:
			break;
	}
	DisplayParameter("Path Loss output", Buffer, DefaultPLParams);
	
	if (PathLossParameters.Type != NONE) {
		// Display the first path loss coordinates
		sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.X1);
		DisplayParameter("Path loss x coordinate 1", Buffer, DefaultPLParams);
		sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.Y1);
		DisplayParameter("Path loss y coordinate 1", Buffer, DefaultPLParams);
		// For routes and grids, display the second path loss coordinates
		if (PathLossParameters.Type != POINT) {
			sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.X2);
			DisplayParameter("Path loss x coordinate 2", Buffer, DefaultPLParams);
			sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.Y2);
			DisplayParameter("Path loss y coordinate 2", Buffer, DefaultPLParams);
		}
		// Display the path loss z coordinate
		sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.Z1);
		if (PathLossParameters.Type != CUBE) {
			DisplayParameter("Path loss z coordinate ", Buffer, DefaultPLParams);
		}
		else {
			DisplayParameter("Path loss z coordinate 1", Buffer, DefaultPLParams);
			sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.Z2);
			DisplayParameter("Path loss z coordinate 2", Buffer, DefaultPLParams);
		}
		// Display path loss spacing
		if (PathLossParameters.Type == ROUTE) {
			sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.Spacing);
			DisplayParameter("Path loss route spacing", Buffer, DefaultPLParams);
		}
		else if (PathLossParameters.Type == GRID) {
			sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.Spacing);
			DisplayParameter("Path loss grid x spacing", Buffer, DefaultPLParams);
			sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.SpacingY);
			DisplayParameter("Path loss grid y spacing", Buffer, DefaultPLParams);
		}
		else if (PathLossParameters.Type == CUBE) {
			sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.Spacing);
			DisplayParameter("Path loss grid x spacing", Buffer, DefaultPLParams);
			sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.SpacingY);
			DisplayParameter("Path loss grid y spacing", Buffer, DefaultPLParams);
			sprintf_s(Buffer, BufferSize, "%f", PathLossParameters.SpacingZ);
			DisplayParameter("Path loss grid z spacing", Buffer, DefaultPLParams);
		}
		
		// Display the path loss filename
		DisplayParameter("Path loss filename", PathLossFilename, DefaultPathLossFilename);
	}
	
	// Display the store timing flag
	DisplayParameter("Store timing values",InputData.PrintTimingInformation.Flag == true ? "true" : "false", InputData.PrintTimingInformation.Default);
	if (InputData.PrintTimingInformation.Flag == true) {
		// Display the timing filename
		DisplayParameter("Timing filename", TimingFilename, DefaultTimingFilename);
	}
	
	free(Buffer);
}


void DisplayParameter(const char *ParameterName, char *ParameterValue, bool DefaultFlag)
{
	if (DefaultFlag == true) {
		printf("Using default of %s for parameter %s\n", ParameterValue, ParameterName);
	}
	else {
		printf("%s = %s\n", ParameterName, ParameterValue);
	}
}


// Process the data read from the input file
bool ProcessConfigParameters(void)
{
	bool Successful = true;

	// Create the project directory if it doesnt already exist
	if (_mkdir(FolderName) == 0) {
		printf("New folder '%s' created for project '%s'\n", FolderName, ProjectName);
	}
	else if (errno != EEXIST) {
		printf("Error creating directory '%s'\n", FolderName);
		Successful = false;
	}	

	return Successful;
}