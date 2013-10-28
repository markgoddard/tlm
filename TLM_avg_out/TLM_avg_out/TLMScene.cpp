/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		27/10/2008
//	File:		TLMScene.cpp
//
/*********************************************************************************************/

// Header files
#include "stdafx.h"
#include "TLMScene.h"
#include "TLMMaths.h"
#include "TLM.h"


// Type definitions

// Container for polygons, allowing for a linked list system

struct Polygon {
				int nVertices;
				Coordinate *Vertices;
				Polygon *NextPolygon;
				};


// Type definition to describe the orientation of the group of polygons
typedef enum {
				Horizontal,
				Vertical
			 } PolygonType;


// Container for groups of polygons
struct PolygonGroup {
						PolygonType Type;
						double Permittivity;
						double Conductivity;
						double TransmissionLoss;
						bool PropagateFlag;
						double Thickness;
						int Priority;
						Polygon *PolygonList;				// Pointer to alinked list of polygon structures
						PolygonGroup *NextPolygonGroup;		// Used for linked lists of polygon groups
					};


// Reference Global variables
extern Node ***Grid;
extern int xSize, ySize, zSize;

// Input parameters
extern char *SceneFilename;
extern double GridSpacing;
extern InputFlags InputData;


// Function prototypes
bool ReadParameters(char **Context, PolygonGroup *PolygonGroupBuffer);
bool ReadThickness(char **Context, PolygonGroup *PolygonGroupBuffer);
bool ReadPriority(char **Context, PolygonGroup *PolygonGroupBuffer);
bool ReadCoordinates(char **Context, Coordinate *CoordinateBuffer);
bool ReadVertices(char *Buffer, Polygon *PolygonBuffer);
PolygonGroup *NewPolygonGroup(PolygonGroup *PolygonGroupBuffer);
Polygon *NewPolygon(void);
void AddPolygonGroupToList(PolygonGroup **pHead, PolygonGroup *Buffer);
void PrintPolygonGroupList(PolygonGroup *Head);
void FreePolygonGroupList(PolygonGroup *Head);
void FreePolygonList(Polygon *Head);
Coordinate FindMaxSize(PolygonGroup *Head);
void AllocateGridMemory(Coordinate MaxCoordinates);
void AddPolygonsToGrid(PolygonGroup *Head);
Polygon *FindIntersection(Polygon *A, Polygon *B, double Thickness);
void AddVerticalPolygon(Polygon *VPolygon, double Thickness, double Permittivity, bool PropagateFlag);
void AddHorizontalPolygon(Polygon *HPolygon, double Thickness, double Permittivity, bool PropagateFlag);
void FillTriangle(xyCoordinate P1, xyCoordinate P2, xyCoordinate P3, double Z, double Thickness, double Permittivity);
void CalculateReflectionTransmissionCoefficients(void);


// The main setup function

// Reads the scene file and stores the information as PolygonGroup and Polygon structures
bool ReadSceneFile(void) 
{
	FILE *SceneFile;				// File pointer for the scene file
	PolygonGroup *Head = NULL;		// Head of the linked list of polygon groups
	int nPolygons = 0;				// Number of polygons read from the scene file
	Coordinate MaxCoordinates;		// The maximum coordinates observed in the scene

	// Attempt to open the scene file
	if (fopen_s(&SceneFile, SceneFilename, "r") != 0) {
		printf("Could not open scene file '%s'\n", SceneFilename);
		return false;
	}
	else {
		// Flags used whilst reading scene file data
		bool EndOfFile;
		bool ReadingParameters;
		bool SuccessfulRead;
		bool FirstGroup;
		
		char *Buffer;				// Text buffer for reading strings from the scene file
		char *Context = NULL;		// Used for getting tokens from the buffer string
		char *Parameter;			// Stores the identifier letter at the start of each scene file string
		char *Comment;				// Used in removing comments from strings
		int LineCount;					// Used for monitoring the position of errors in the scene file
		Coordinate CoordinateBuffer;	// Stores coordinates read from the buffer
		Coordinate Offset;						// Stores the current coordinate offset
		PolygonGroup *PolygonGroupBuffer;		// Stores polygon group parameters as they are read from the file
		Polygon *PolygonBuffer;					// Stores polygon parameters as they are read from the file
		Polygon **PolygonListTail;				// Tail of the linked list of polygons

		// Allocate memory for a new polygon group and the coordinate buffer
		PolygonGroupBuffer = NewPolygonGroup(NULL);
		PolygonListTail = &PolygonGroupBuffer->PolygonList;
		
		// Initiate the buffers
		CoordinateBuffer.X = 0;
		CoordinateBuffer.Y = 0;
		CoordinateBuffer.Z = 0;
		Offset.X = 0;
		Offset.Y = 0;
		Offset.Z = 0;

		// Allocate memory for the buffer
		Buffer = (char*) malloc(200*sizeof(char));

		// Initialise the flags
		EndOfFile = false;
		ReadingParameters = false;
		SuccessfulRead = true;
		FirstGroup = true;

		// Initialise the line count to 0
		LineCount = 0;

		printf("\nReading polygon information from scene file '%s'\n\n", SceneFilename);

		// Read the scene file, one line at a time
		do {
			if (fgets(Buffer, 200, SceneFile) != NULL) {
								
				// Keep track of the line number
				LineCount++;
				// Search for and remove comments				
				Comment = strstr(Buffer, "//");
				if (Comment != NULL) {
					Comment[0] = NULL;	// Replace with 0 to mark a new end of the string
				}

				// Remove leading whitespace and read the parameter type character (p,t,z,o,h,v)
				Parameter = strtok_s(Buffer, "\t ", &Context);
				if (Parameter != NULL) {
					switch (Parameter[0]) {
						
						// Read the electrical parameters
						case 'p':
							// Set the reading parameters flag, create a new polygon group and add the old one to the list
							ReadingParameters = true;
							// Add the polygon buffer to the list if this is not the first read
							if (FirstGroup == true) {
								FirstGroup = false;
							}
							else {
								AddPolygonGroupToList(&Head, PolygonGroupBuffer);
								PolygonGroupBuffer = NewPolygonGroup(PolygonGroupBuffer);						
								PolygonListTail = &PolygonGroupBuffer->PolygonList;
								// FIXME: Should the offset be reset upon reading a 'p'?
								//Offset.X = 0;
								//Offset.Y = 0;
								//Offset.Z = 0;
							}
							SuccessfulRead = ReadParameters(&Context, PolygonGroupBuffer);
							break;
						
						// Read the wall thickness
						case 't':
							// If not currently reading parameters (i.e. not just read a p, z or t) create a new polygon group and add the old one to the list
							if (ReadingParameters == false) {
								ReadingParameters = true;
								AddPolygonGroupToList(&Head, PolygonGroupBuffer);
								PolygonGroupBuffer = NewPolygonGroup(PolygonGroupBuffer);
								PolygonListTail = &PolygonGroupBuffer->PolygonList;
								// FIXME: Should the offset be reset upon reading a 't'?
								//Offset.X = 0;
								//Offset.Y = 0;
								//Offset.Z = 0;
							}
							SuccessfulRead = ReadThickness(&Context, PolygonGroupBuffer);
							break;
						
						// Read the priority
						case 'z':
							// If not currently reading parameters (i.e. not just read a p, z or t) create a new polygon group and add the old one to the list
							if (ReadingParameters == false) {
								ReadingParameters = true;
								AddPolygonGroupToList(&Head, PolygonGroupBuffer);
								PolygonGroupBuffer = NewPolygonGroup(PolygonGroupBuffer);
								PolygonListTail = &PolygonGroupBuffer->PolygonList;
								// FIXME: Should the offset be reset upon reading a 'z'?
								//Offset.X = 0;
								//Offset.Y = 0;
								//Offset.Z = 0;
							}
							SuccessfulRead = ReadPriority(&Context, PolygonGroupBuffer);
							break;
						
						// Read the offset
						case 'o':
							SuccessfulRead = ReadCoordinates(&Context, &Offset);
							break;

						// Read a vertical polygon
						case 'v':
							// Ensure any previous polygons were also vertical
							if (ReadingParameters == false && PolygonGroupBuffer->Type != Vertical) {
								AddPolygonGroupToList(&Head, PolygonGroupBuffer);
								PolygonGroupBuffer = NewPolygonGroup(PolygonGroupBuffer);
								PolygonListTail = &PolygonGroupBuffer->PolygonList;
							}
							ReadingParameters = false;
							PolygonGroupBuffer->Type = Vertical;
							// Allocate memory for a new polygon structure with 2 vertices
							PolygonBuffer = NewPolygon();
							PolygonBuffer->Vertices = (Coordinate*)malloc(2*sizeof(Coordinate));
							// Read in 2 sets of coordinates
							while (ReadCoordinates(&Context, &CoordinateBuffer) == true) {
								// Increment the number of vertices and  ensure only 2 are read for a vertical polygon
								if (PolygonBuffer->nVertices < 2) {
									PolygonBuffer->Vertices[PolygonBuffer->nVertices].X = CoordinateBuffer.X + Offset.X;
									PolygonBuffer->Vertices[PolygonBuffer->nVertices].Y = CoordinateBuffer.Y + Offset.Y;
									PolygonBuffer->Vertices[PolygonBuffer->nVertices].Z = CoordinateBuffer.Z + Offset.Z;
								}	
								PolygonBuffer->nVertices++;
							}
							// Ensure both both vertices have been read
							if (PolygonBuffer->nVertices != 2) {
								SuccessfulRead = false;
							}
							else {
								nPolygons++;
								*PolygonListTail = PolygonBuffer;
								PolygonListTail = &(*PolygonListTail)->NextPolygon;
							}
							break;

						// Read a horizontal polygon
						case 'h':
							// Ensure any previous polygons were also horizontal
							if (ReadingParameters == false && PolygonGroupBuffer->Type != Horizontal) {
								AddPolygonGroupToList(&Head, PolygonGroupBuffer);
								PolygonGroupBuffer = NewPolygonGroup(PolygonGroupBuffer);
								PolygonListTail = &PolygonGroupBuffer->PolygonList;
							}
							ReadingParameters = false;
							PolygonGroupBuffer->Type = Horizontal;
							// Allocate memory for a new polygon structure with 2 vertices
							PolygonBuffer = NewPolygon();
							PolygonBuffer->Vertices = (Coordinate*)malloc(50*sizeof(Coordinate));
							// Read in at least 3 sets of coordinates
							while (ReadCoordinates(&Context, &CoordinateBuffer) == true) {
								// Increment the number of vertices and ensure there is enough memory for all of the vertices, reallocating 10 vertices at a time
								PolygonBuffer->Vertices[PolygonBuffer->nVertices].X = CoordinateBuffer.X + Offset.X;
								PolygonBuffer->Vertices[PolygonBuffer->nVertices].Y = CoordinateBuffer.Y + Offset.Y;
								PolygonBuffer->Vertices[PolygonBuffer->nVertices].Z = CoordinateBuffer.Z + Offset.Z;
								PolygonBuffer->nVertices++;
							}
							// Ensure at least 3 vertices have been read
							if (PolygonBuffer->nVertices < 3) {
								SuccessfulRead = false;
							}
							else {
								nPolygons++;
								*PolygonListTail = PolygonBuffer;
								PolygonListTail = &(*PolygonListTail)->NextPolygon;
							}
							PolygonBuffer->Vertices = (Coordinate*)realloc(PolygonBuffer->Vertices, PolygonBuffer->nVertices*sizeof(Coordinate));
							break;
					}
				}
			}
			else if (feof(SceneFile) != 0) {
				EndOfFile = true;
			}
		}
		while (EndOfFile == false && SuccessfulRead == true);

		// Add the final group to the list
		AddPolygonGroupToList(&Head, PolygonGroupBuffer);
								
		// Free allocated memory
		free(Buffer);
		fclose(SceneFile);

		// Check to see if there were any errors in the scene file
		if (SuccessfulRead == false) {
			printf("Error in scene file, line %d\n", LineCount);
			// Free memory allocated to the polygons
			FreePolygonGroupList(Head);
			return false;
		}
		else {
			printf("Scene file parsed successfully\nRead %d polygons\n\n", nPolygons);
		}
	}

	// Find the maximum coordinates in the system and allocate enough memory for a rectangle of this size
	MaxCoordinates = FindMaxSize(Head);
	// Print information on the polygons to the display
	if (InputData.DisplayPolygonInformation.Flag == true) {
		PrintPolygonGroupList(Head);
	}
	// Allocate memory for the TLM grid
	AllocateGridMemory(MaxCoordinates);
	// Add the polygons into the grid
	AddPolygonsToGrid(Head);
	// Free memory allocated to the polygons
	FreePolygonGroupList(Head);
	// Calculate the reflection and transmission coefficients based on their impedances
	CalculateReflectionTransmissionCoefficients();

	return true;
}

// Read a string containing polygon parameters and store in PolygonGroupBuffer 
bool ReadParameters(char **Context, PolygonGroup *PolygonGroupBuffer)
{
	bool Successful;
	char *Parameter;

	Successful = true;

	// Read conductivity
	Parameter = strtok_s(NULL, "\t ", Context);
	if (Parameter != NULL) {
		PolygonGroupBuffer->Conductivity = atof(Parameter);
		if (PolygonGroupBuffer->Conductivity < 0) {
			Successful = false;
		}
	}
	else {
		Successful = false;
	}
	// Read permittivity
	Parameter = strtok_s(NULL, "\t ", Context);
	if (Parameter != NULL) {
		PolygonGroupBuffer->Permittivity = atof(Parameter);
		if (PolygonGroupBuffer->Permittivity < 0) {
			Successful = false;
		}
	}
	else {
		Successful = false;
	}

	// Read transmission loss
	Parameter = strtok_s(NULL, "\t ", Context);
	if (Parameter != NULL) {
		PolygonGroupBuffer->TransmissionLoss = atof(Parameter);
		if (PolygonGroupBuffer->TransmissionLoss < 0) {
			Successful = false;
		}
	}
	else {
		Successful = false;
	}

	// Read propagate flag
	Parameter = strtok_s(NULL, "\t ", Context);
	if (Parameter != NULL) {
		if (Parameter[0] == '1') {
			PolygonGroupBuffer->PropagateFlag = true;
		}
		else if (Parameter[0] == '0') {
			PolygonGroupBuffer->PropagateFlag = false;
		}	
		else {
			Successful = false;
		}
	}
	else {
		Successful = false;
	}

	return Successful;
}


// Read a string containing polygon thickness parameter and store in PolygonGroupBuffer
bool ReadThickness(char **Context, PolygonGroup *PolygonGroupBuffer)
{
	bool Successful;
	char *Parameter;

	Successful = true;

	// Read thickness
	Parameter = strtok_s(NULL, "\t ", Context);
	if (Parameter != NULL) {
		PolygonGroupBuffer->Thickness = atof(Parameter);
		if (PolygonGroupBuffer->Thickness < 0) {
			Successful = false;
		}
	}
	else {
		Successful = false;
	}

	return Successful;
}


// Read a string containing polygon priority parameter and store in PolygonGroupBuffer
bool ReadPriority(char **Context, PolygonGroup *PolygonGroupBuffer)
{
	bool Successful;
	char *Parameter;

	Successful = true;

	// Read priority
	Parameter = strtok_s(NULL, "\t ", Context);
	if (Parameter != NULL) {
		PolygonGroupBuffer->Priority = atoi(Parameter);
		if (PolygonGroupBuffer->Priority < 0) {
			Successful = false;
		}
	}
	else {
		Successful = false;
	}

	return Successful;
}


// Read a string containing a set of coordinates and store in CoordinateBuffer
bool ReadCoordinates(char **Context, Coordinate *CoordinateBuffer)
{
	bool Successful;
	char *Parameter;

	Successful = true;
				
	// Read x coordinate
	Parameter = strtok_s(NULL, "\t ", Context);
	if (Parameter != NULL) {
		CoordinateBuffer->X = atof(Parameter);
	}
	else {
		Successful = false;
	}
	// Read y coordinate
	Parameter = strtok_s(NULL, "\t ", Context);
	if (Parameter != NULL) {
		CoordinateBuffer->Y = atof(Parameter);
	}
	else {
		Successful = false;
	}
	// Read z coordinate
	Parameter = strtok_s(NULL, "\t ", Context);
	if (Parameter != NULL) {
		CoordinateBuffer->Z = atof(Parameter);
	}
	else {
		Successful = false;
	}

	return Successful;
}


// Allocate memory for and initiate a polygon group structure, initialising to PolygonGroupBuffer or default values if PolygonGroupBuffer is NULL
PolygonGroup *NewPolygonGroup(PolygonGroup *PolygonGroupBuffer)
{
	PolygonGroup *NewPolygonGroup;

	NewPolygonGroup = (PolygonGroup*) malloc(sizeof(PolygonGroup));

	// New Polygon required
	if (PolygonGroupBuffer == NULL) {
		NewPolygonGroup->Type = Horizontal;
		NewPolygonGroup->Permittivity = 0;
		NewPolygonGroup->Conductivity = 0;
		NewPolygonGroup->TransmissionLoss = 0;
		NewPolygonGroup->PropagateFlag = false;
		NewPolygonGroup->Thickness = 0;
		NewPolygonGroup->Priority = 0;
	}
	// Copy the polygon from another
	else {
		NewPolygonGroup->Type = PolygonGroupBuffer->Type;
		NewPolygonGroup->Permittivity = PolygonGroupBuffer->Permittivity;
		NewPolygonGroup->Conductivity = PolygonGroupBuffer->Conductivity;
		NewPolygonGroup->TransmissionLoss = PolygonGroupBuffer->TransmissionLoss;
		NewPolygonGroup->PropagateFlag = PolygonGroupBuffer->PropagateFlag;
		NewPolygonGroup->Thickness = PolygonGroupBuffer->Thickness;
		NewPolygonGroup->Priority = PolygonGroupBuffer->Priority;
	}

	NewPolygonGroup->PolygonList = NULL;
	NewPolygonGroup->NextPolygonGroup = NULL;

	return NewPolygonGroup;
}


// Allocate memory for and initiate a polygon structure, initialising to default values
Polygon *NewPolygon(void)
{
	Polygon *NewPolygon;

	NewPolygon = (Polygon*)malloc(sizeof(Polygon));
	NewPolygon->nVertices = 0;
	NewPolygon->Vertices = NULL;
	NewPolygon->NextPolygon = NULL;

	return NewPolygon;
}


// Add a polygon group to the linked list of polygon groups pHead. The list stores polygon groups in order of priority such that the head is of lowest priority (usually 0)
void AddPolygonGroupToList(PolygonGroup **pHead, PolygonGroup *PolygonGroupBuffer) 
{
	if (*pHead == NULL) {
		*pHead = PolygonGroupBuffer;
		(*pHead)->NextPolygonGroup = NULL;
	}
	else {
		PolygonGroup *PreviousPolygonGroup;
		PolygonGroup *NextPolygonGroup;

		NextPolygonGroup = *pHead;
		PreviousPolygonGroup = *pHead;

		// Search for the first polygon of equal or higher priority
		while (PreviousPolygonGroup->NextPolygonGroup != NULL && NextPolygonGroup->Priority <= PolygonGroupBuffer->Priority) {
			PreviousPolygonGroup = NextPolygonGroup;
			NextPolygonGroup = PreviousPolygonGroup->NextPolygonGroup;
		}
		// Insert the new polygon into the list
		PolygonGroupBuffer->NextPolygonGroup = PreviousPolygonGroup->NextPolygonGroup;
		PreviousPolygonGroup->NextPolygonGroup = PolygonGroupBuffer;
	}
}


// Free memory allocated to a linked list of polygon group structures, freeing each polygon group in turn
void FreePolygonGroupList(PolygonGroup *Head)
{
	PolygonGroup *PolygonGroupPtr;
	PolygonGroup *NextPolygonGroup;
	
	// Free each polygon group in turn 
	PolygonGroupPtr = Head;
	while (PolygonGroupPtr != NULL) {
		NextPolygonGroup = PolygonGroupPtr->NextPolygonGroup;
		FreePolygonList(PolygonGroupPtr->PolygonList);
		free(PolygonGroupPtr);
		PolygonGroupPtr = NextPolygonGroup;
	}
}


// Free memory allocated to a linked list of polygon structures
void FreePolygonList(Polygon *Head)
{
	Polygon *PolygonPtr;
	Polygon *NextPolygon;

	// Free each polygon in turn
	PolygonPtr = Head;
	while (PolygonPtr != NULL) {
		NextPolygon = PolygonPtr->NextPolygon;
		free(PolygonPtr->Vertices);
		free(PolygonPtr);
		PolygonPtr = NextPolygon;
	}
}


// Print the parameters of each polygon group to the display, followed by the details of any polygons in each group
void PrintPolygonGroupList(PolygonGroup *Head)
{
	int i = 1;
	PolygonGroup *PolygonGroupPtr = Head;
	while (PolygonGroupPtr != NULL) {
		printf("\nPolygon group %d parameters:\n", i);
		printf("Type = %s\nPerm = %f\nCond = %f\nTL = %f\nPropagate = %s\nThickness = %f\nPriority = %d\n\n", PolygonGroupPtr->Type == Horizontal ? "horizontal" : "vertical", PolygonGroupPtr->Permittivity,PolygonGroupPtr->Conductivity, PolygonGroupPtr->TransmissionLoss, PolygonGroupPtr->PropagateFlag == true ? "true" : "false", PolygonGroupPtr->Thickness, PolygonGroupPtr->Priority);
		
		// Print each of the polygons
		Polygon *PolygonPtr = PolygonGroupPtr->PolygonList;
		int j = 1;
		while (PolygonPtr != NULL) {
			printf("Polygon %d parameters:\n", j);
			// Print each of the vertices in turn
			for (int k = 0; k<PolygonPtr->nVertices; k++) {
				printf("Vertex %d:\tX = %.3f\tY = %.3f\tZ = %.3f\n", k+1, PolygonPtr->Vertices[k].X, PolygonPtr->Vertices[k].Y, PolygonPtr->Vertices[k].Z);
			}
			// Find the next polygon in the list
			j++;
			PolygonPtr = PolygonPtr->NextPolygon;
		}
		// find the next polygon group in the list
		PolygonGroupPtr = PolygonGroupPtr->NextPolygonGroup;
		i++;
	}
}


// Find the maximum coordinates out of all of the polygons and hence find the grid sizes xSize, ySize and zSize
Coordinate FindMaxSize(PolygonGroup *Head)
{
	Coordinate MaxCoordinates;
	PolygonGroup *PolygonGroupPtr = Head;

	MaxCoordinates.X = 0;
	MaxCoordinates.Y = 0;
	MaxCoordinates.Z = 0;

	while (PolygonGroupPtr != NULL) {
		Polygon *PolygonPtr = PolygonGroupPtr->PolygonList;

		while (PolygonPtr != NULL) {
			for (int i=0; i<PolygonPtr->nVertices; i++) {
				if (PolygonPtr->Vertices[i].X > MaxCoordinates.X) {
					MaxCoordinates.X = PolygonPtr->Vertices[i].X;
				}
				if (PolygonPtr->Vertices[i].Y > MaxCoordinates.Y) {
					MaxCoordinates.Y = PolygonPtr->Vertices[i].Y;
				}
				if (PolygonPtr->Vertices[i].Z > MaxCoordinates.Z) {
					MaxCoordinates.Z = PolygonPtr->Vertices[i].Z;
				}
			}
			PolygonPtr = PolygonPtr->NextPolygon;
		}
		PolygonGroupPtr = PolygonGroupPtr->NextPolygonGroup;
	}
	printf("Maximum grid size:\tX = %.2f\tY = %.2f\tZ = %.2f\n", MaxCoordinates.X, MaxCoordinates.Y, MaxCoordinates.Z);

	return MaxCoordinates;
}


// Allocate enough memory for all of the nodes in the TLM grid based upon xSize, ySize and zSize
void AllocateGridMemory(Coordinate MaxCoordinates)
{
	xSize = RoundUpwards(MaxCoordinates.X/GridSpacing)+1;
	ySize = RoundUpwards(MaxCoordinates.Y/GridSpacing)+1;
	zSize = RoundUpwards(MaxCoordinates.Z/GridSpacing)+1;

	// Allocate memory for the nodes
	Grid = (Node***) malloc(xSize * sizeof(Node**));
	for (int x = 0; x < xSize; x++) {
		Grid[x] = (Node**) malloc(ySize * sizeof(Node*));
		for (int y = 0; y < ySize; y++) {
			Grid[x][y] = (Node*) malloc(zSize * sizeof(Node));
		}
	}

	// Set all nodes to 0
	for (int x = 0; x < xSize; x++) {
		for (int y = 0; y < ySize; y++) {
			for (int z = 0; z < zSize; z++) {
				Grid[x][y][z].V = 0;
				Grid[x][y][z].VxpIn = 0;
				Grid[x][y][z].VxnIn = 0;
				Grid[x][y][z].VypIn = 0;
				Grid[x][y][z].VynIn = 0;
				Grid[x][y][z].VzpIn = 0;
				Grid[x][y][z].VznIn = 0;
				Grid[x][y][z].VxpOut = 0;
				Grid[x][y][z].VxnOut = 0;
				Grid[x][y][z].VypOut = 0;
				Grid[x][y][z].VynOut = 0;
				Grid[x][y][z].VzpOut = 0;
				Grid[x][y][z].VznOut = 0;
				Grid[x][y][z].Vmax = 0;
				Grid[x][y][z].Vavg = 0;
				Grid[x][y][z].Z = IMPEDANCE_OF_FREE_SPACE;
				Grid[x][y][z].PropagateFlag = true;
				Grid[x][y][z].Active = false;
			}
		}
	}
}
// Use the list of polygons to generate the relevant impedances in the TLM grid
void AddPolygonsToGrid(PolygonGroup *Head)
{
	PolygonGroup *PolygonGroupPtr;
	Polygon *PolygonPtr;

	PolygonGroupPtr = Head;

	while (PolygonGroupPtr != NULL) {
		PolygonPtr = PolygonGroupPtr->PolygonList;
		if (PolygonGroupPtr->Type == Vertical) {
			// Add vertical polygons into the grid
			while (PolygonPtr != NULL) {
				// Check all polygons with a lower priority for intersections
				PolygonGroup *IntersectionTestGroup = Head;
				Polygon *IntersectionTest;
				// Ensure only vertical polygons of lower priority and greater thickness are checked
				while (IntersectionTestGroup != NULL && IntersectionTestGroup->Type == Vertical && IntersectionTestGroup->Priority < PolygonGroupPtr->Priority && IntersectionTestGroup->Thickness > PolygonGroupPtr->Thickness) {
					IntersectionTest = IntersectionTestGroup->PolygonList;
					while(IntersectionTest != NULL) {
						// Find the intersection of the two polygons, if there is one
						Polygon *Intersection;
						Intersection = FindIntersection(IntersectionTest, PolygonPtr, IntersectionTestGroup->Thickness);
						if (Intersection != NULL) {
							// Add an air gap the size of the intersection to the grid
							AddVerticalPolygon(Intersection, IntersectionTestGroup->Thickness, 1.0, true);
							free(Intersection);
						}
						IntersectionTest = IntersectionTest->NextPolygon;
					}
					IntersectionTestGroup = IntersectionTestGroup->NextPolygonGroup;
				}
				AddVerticalPolygon(PolygonPtr, PolygonGroupPtr->Thickness, PolygonGroupPtr->Permittivity, PolygonGroupPtr->PropagateFlag);
				PolygonPtr = PolygonPtr->NextPolygon;
			}
		}
		else {
			// Add horizontal polygons into the grid
			while (PolygonPtr != NULL) {
				AddHorizontalPolygon(PolygonPtr, PolygonGroupPtr->Thickness, PolygonGroupPtr->Permittivity, PolygonGroupPtr->PropagateFlag);
				PolygonPtr = PolygonPtr->NextPolygon;
			}
		}		
		PolygonGroupPtr = PolygonGroupPtr->NextPolygonGroup;
	}
}


// Find the intersection of two polygons, return NULL if there is no intersection
Polygon *FindIntersection(Polygon *A, Polygon *B, double Thickness)
{
	Polygon *Intersection;
	xyCoordinate A1, A2;
	xyCoordinate B1, B2;
	xyLine L;

	Intersection = (Polygon*)malloc(sizeof(Polygon));
	Intersection->Vertices = (Coordinate*)malloc(2*sizeof(Coordinate));

	// Find the minimum and maximum z coordinates for each Polygon
	Intersection->Vertices[0].Z = MAX(MIN(A->Vertices[0].Z, A->Vertices[1].Z), MIN(B->Vertices[0].Z, B->Vertices[1].Z));
	Intersection->Vertices[1].Z = MIN(MAX(A->Vertices[0].Z, A->Vertices[1].Z), MAX(B->Vertices[0].Z, B->Vertices[1].Z));
	
	// Ensure the polygons intersect in the z dimension
	if (Intersection->Vertices[0].Z < Intersection->Vertices[1].Z) {
		A1 = CoordinateToXY(A->Vertices[0]);
		A2 = CoordinateToXY(A->Vertices[1]);
		B1 = CoordinateToXY(B->Vertices[0]);
		B2 = CoordinateToXY(B->Vertices[1]);

		L = CoordinatesToLine(A1, A2);

		// Check that the two points lie on the line joining the vertices of A
		if (abs(DistanceFromLine(L,B1)) < Thickness/2 && abs(DistanceFromLine(L,B2)) < Thickness/2) {
			Intersection->Vertices[0].X = MAX(MIN(A->Vertices[0].X, A->Vertices[1].X), MIN(B->Vertices[0].X, B->Vertices[1].X));
			Intersection->Vertices[1].X = MIN(MAX(A->Vertices[0].X, A->Vertices[1].X), MAX(B->Vertices[0].X, B->Vertices[1].X));
			// Ensure polygons intersect in the X direction
			if (Intersection->Vertices[0].X < Intersection->Vertices[1].X) {
				Intersection->Vertices[0].Y = YFromX(L,Intersection->Vertices[0].X);
				Intersection->Vertices[1].Y = YFromX(L,Intersection->Vertices[1].X);
			}
			else if (Intersection->Vertices[0].X == Intersection->Vertices[1].X) {
				Intersection->Vertices[0].Y = MAX(MIN(A->Vertices[0].Y, A->Vertices[1].Y), MIN(B->Vertices[0].Y, B->Vertices[1].Y));
				Intersection->Vertices[1].Y = MIN(MAX(A->Vertices[0].Y, A->Vertices[1].Y), MAX(B->Vertices[0].Y, B->Vertices[1].Y));
			}
			else {
				free(Intersection);
				Intersection = NULL;
			}			
		}
		else {
			free(Intersection);
			Intersection = NULL;
		}
	}
	else {
		free(Intersection);
		Intersection = NULL;
	}

	return Intersection;
}


// Add a vertical polygon to the TLM grid
void AddVerticalPolygon(Polygon *VPolygon, double Thickness, double Permittivity, bool PropagateFlag)
{
	xyCoordinate P1 = {VPolygon->Vertices[0].X, VPolygon->Vertices[0].Y};
	xyCoordinate P2 = {VPolygon->Vertices[1].X, VPolygon->Vertices[1].Y};
	xyCoordinate Vertices[4];
	xyLine Sides[4];
	xyLine CentralLine;
	double SineTheta, CosineTheta;
	double dx, dy;
	double Impedance;
	int xMin, xMax;
	int yMin, yMax;
	int zMin, zMax;

	// Set the impedance variable
	Impedance = IMPEDANCE_OF_FREE_SPACE/sqrt(Permittivity);

	// Find the minimum and maximum nodes in the z-direction
	if (VPolygon->Vertices[0].Z < VPolygon->Vertices[1].Z) {
		zMin = RoundUpwards(VPolygon->Vertices[0].Z/GridSpacing);
		zMax = (int)(VPolygon->Vertices[1].Z/GridSpacing);
	}
	else {
		zMin = RoundUpwards(VPolygon->Vertices[1].Z/GridSpacing);
		zMax = (int)(VPolygon->Vertices[0].Z/GridSpacing);
	}
	zMin = PlaceWithinGridZ(zMin);
	zMax = PlaceWithinGridZ(zMax);

	// Ensure the smallest X-coordinate is in P1
	if (P1.X > P2.X) {
		// Swap the coordinates
		xyCoordinate P = P1;
		P1 = P2;
		P2 = P;
	}
	// For vertical lines ensure the first y coordinate is greatest
	else if (P1.X == P2.X) {
		if (P1.Y < P2.Y) {
			// Swap the coordinates
			xyCoordinate P = P1;
			P1 = P2;
			P2 = P;
		}
	}

	// Find the equation of the line connecting the two points
	CentralLine = CoordinatesToLine(P1,P2);
	SineTheta = SineThetaFromLine(CentralLine);
	CosineTheta = CosineThetaFromLine(CentralLine);

	// Ensure the rectangle is at least as wide as the grid spacing
	if (Thickness < GridSpacing) {
		Thickness = GridSpacing;
	}

	// Find the vertices of the rectangle
	dx = abs(Thickness*SineTheta/2);
	dy = abs(Thickness*CosineTheta/2);

	// Find the vertices of the rectangle

	// Outer most x terms always the same
	Vertices[0].X = P1.X - dx;
	Vertices[3].X = P2.X + dx;

	// Check the gradient of the line
	if (CentralLine.xCoeff*CentralLine.yCoeff < 0 || CentralLine.xCoeff == 0) {
		// Positive or zero gradient
		Vertices[0].Y = P1.Y + dy;
		Vertices[1].X = P1.X + dx;
		Vertices[1].Y = P1.Y - dy;
		Vertices[2].X = P2.X - dx;
		Vertices[2].Y = P2.Y + dy;
		Vertices[3].Y = P2.Y - dy;
	}
	else {
		// Negative or infinite gradient
		Vertices[0].Y = P1.Y - dy;
		Vertices[1].X = P2.X - dx;
		Vertices[1].Y = P2.Y - dy;
		Vertices[2].X = P1.X + dx;
		Vertices[2].Y = P1.Y + dy;
		Vertices[3].Y = P2.Y + dy;
	}
	
	// Find the sides of the rectangle
	Sides[0] = CoordinatesToLine(Vertices[0],Vertices[1]);
	Sides[1] = CoordinatesToLine(Vertices[0],Vertices[2]);
	Sides[2] = CoordinatesToLine(Vertices[1],Vertices[3]);
	Sides[3] = CoordinatesToLine(Vertices[2],Vertices[3]);

	// Find all the points between lines S0 and S1, from x coordinates of P0.x to P1.x
	xMin = RoundUpwards(Vertices[0].X/GridSpacing);
	xMin = PlaceWithinGridX(xMin);
	xMax = MIN((int)(Vertices[1].X/GridSpacing),(int)(Vertices[2].X/GridSpacing));
	xMax = PlaceWithinGridX(xMax);
	if (Sides[0].yCoeff != 0) {
		for (int x = xMin; x <= xMax; x++) {
			// Set the minimum and maximum y coordinates for this vertical strip of the rectangle
			yMin = RoundUpwards(YFromX(Sides[0], x*GridSpacing)/GridSpacing);
			yMin = PlaceWithinGridY(yMin);
			yMax = (int) (YFromX(Sides[1],x*GridSpacing)/GridSpacing);
			yMax = PlaceWithinGridY(yMax);

			for (int y = yMin; y <= yMax; y++) {
				// Repeat for all z-coordinates within the height of the polygon
				for (int z = zMin; z <= zMax; z++) {
					Grid[x][y][z].Z = Impedance;
					Grid[x][y][z].PropagateFlag = PropagateFlag;
				}
			}
		}
	}

	// Find all the points between lines S2 and S1, from x coordinates of P1.x to P2.x
	xMin = MIN(RoundUpwards(Vertices[1].X/GridSpacing),RoundUpwards(Vertices[2].X/GridSpacing));
	xMin = PlaceWithinGridX(xMin);
	xMax = MAX((int)(Vertices[1].X/GridSpacing),(int)(Vertices[2].X/GridSpacing));
	xMax = PlaceWithinGridX(xMax);

	for (int x = xMin; x <= xMax; x++) {
		// Set the minimum and maximum y coordinates for this vertical strip of the rectangle
		if (Vertices[1].X <= Vertices[2].X) {
			yMin = RoundUpwards(YFromX(Sides[2], x*GridSpacing)/GridSpacing);
			yMax = (int) (YFromX(Sides[1],x*GridSpacing)/GridSpacing);
		}
		else {
			yMin = RoundUpwards(YFromX(Sides[0], x*GridSpacing)/GridSpacing);
			yMax = (int) (YFromX(Sides[3],x*GridSpacing)/GridSpacing);
		}
		yMin = PlaceWithinGridY(yMin);
		yMax = PlaceWithinGridY(yMax);
		
		for (int y = yMin; y <= yMax; y++) {
			// Repeat for all z-coordinates within the height of the polygon
			for (int z = zMin; z <= zMax; z++) {
				Grid[x][y][z].Z = Impedance;
				Grid[x][y][z].PropagateFlag = PropagateFlag;
			}
		}
	}

	// Find all the points between lines S2 and S3, from x coordinates of P2.x to P3.x
	if (Sides[3].yCoeff != 0) {
		xMin = MAX(RoundUpwards(Vertices[1].X/GridSpacing),RoundUpwards(Vertices[2].X/GridSpacing));
		xMin = PlaceWithinGridX(xMin);
		xMax = (int)(Vertices[3].X/GridSpacing);
		xMax = PlaceWithinGridX(xMax);

		for (int x = xMin; x <= xMax; x++) {
			// Set the minimum and maximum y coordinates for this vertical strip of the rectangle
			yMin = RoundUpwards(YFromX(Sides[2], x*GridSpacing)/GridSpacing);
			yMin = PlaceWithinGridY(yMin);
			yMax = (int) (YFromX(Sides[3],x*GridSpacing)/GridSpacing);
			yMax = PlaceWithinGridY(yMax);

			for (int y = yMin; y <= yMax; y++) {
				// Repeat for all z-coordinates within the height of the polygon
				for (int z = zMin; z <= zMax; z++) {
					Grid[x][y][z].Z = Impedance;
					Grid[x][y][z].PropagateFlag = PropagateFlag;
				}
			}
		}
	}
}


// Add a horizontal polygon to the TLM grid
void AddHorizontalPolygon(Polygon *HPolygon, double Thickness, double Permittivity, bool PropagateFlag)
{
	int nVertices = HPolygon->nVertices;
	int VerticesRemaining;
	int Direction;
	double *Angles;
	double TotalAngle = 0;
	xyCoordinate *Vertices;
	xyCoordinate **Triangles;
	xyLine *Sides;
	bool *VertexRemoved;

	// Allocate memory for the angles, vertices, triangles and sides of the polygon
	Angles = (double*)malloc(nVertices*sizeof(double));
	Vertices = (xyCoordinate*)malloc(nVertices*sizeof(xyCoordinate));
	Triangles = (xyCoordinate**)malloc((nVertices-2)*sizeof(xyCoordinate*));
	for (int i=0; i < (nVertices - 2); i++) {
		Triangles[i] = (xyCoordinate*)malloc(3*sizeof(xyCoordinate));
	}
	Sides = (xyLine*)malloc(nVertices*sizeof(xyLine));
	VertexRemoved = (bool*)malloc(nVertices*sizeof(bool));

	// Store the xy coordinates of the vertices of the polygon in xyCoordinate structures
	for (int i = 0; i < nVertices; i++) {
		Vertices[i].X = HPolygon->Vertices[i].X;
		Vertices[i].Y = HPolygon->Vertices[i].Y;
	}

	// Costruct the equations of the sides of the polygon from its vertices
	for (int i = 0; i < nVertices; i++) {
		Sides[i] = CoordinatesToLine(Vertices[i], Vertices[(i+1)%nVertices]);
	}

	// Find the angles between each of the side vectors
	for (int i = 0; i < nVertices; i++) {
		Angles[i] = AngleBetweenLines(Sides[(i+nVertices-1)%nVertices],Sides[i]);
		TotalAngle += Angles[i];
	}

	// Convert the angles between the side vectors into the inner angles of the polygon
	if (TotalAngle > 0) {
		// Points specified in anti-clockwise direction
		Direction = 1;
	}
	else {
		// Points specified in clockwise direction
		Direction = -1;
	}

	// Reduce the polygon to triangles, removing one vertex at a time
	int CurrentVertex = 0;	// Count variable
	int Previous;		// The previous and next valid vertices in the polygon
	int Next;

	// Set the number of remaining vertices to the initial order of the polygon
	VerticesRemaining = nVertices;
	// Set all of the removed flags to false
	for (int i = 0; i < nVertices; i++) {
		VertexRemoved[i] = false;
	}


	// Remove triangles one by one
	while (VerticesRemaining > 2) {
		if (VertexRemoved[CurrentVertex] == false) {
			// Check if the angle is reflex (adjusting for clockwise or anticlockwise points with 'Direction'
			if (Angles[CurrentVertex]*Direction > 0) {
				Previous = CurrentVertex;
				Next = CurrentVertex;
				// Find the next vertices in both directions that have not been removed
				do {
					Previous = (Previous + nVertices - 1)%nVertices;
				}
				while (VertexRemoved[Previous] == true);
				do {
					Next = (Next+1)%nVertices;
				}
				while (VertexRemoved[Next] == true);

				// Find the equation of the potential new side of the polygon
				xyLine NewSide = CoordinatesToLine(Vertices[Previous], Vertices[Next]);			// Add a new side joining the previous and next vertices

				// Ensure the triangle formed does not enclose any other points
				bool EnclosesOtherPoints = false;

				for (int i=0; i<nVertices; i++) {
					if (VertexRemoved[i] == false && i != CurrentVertex && i != Next && i!= Previous) {
						if (IsWithinTriangle(NewSide, Sides[CurrentVertex], Sides[Previous], Vertices[i], Direction) == true) {
							EnclosesOtherPoints = true;
						}
					}
				}

				if (EnclosesOtherPoints == false) {
					// Fill in the current triangle
					FillTriangle(Vertices[CurrentVertex], Vertices[Previous], Vertices[Next], HPolygon->Vertices[0].Z, Thickness, Permittivity);

					// Reconstruct the polygon sides and angles following the removal of a vertex
				
					Sides[(Next+nVertices-1)%nVertices] = NewSide;		// Replace both of the old sides
					Sides[Previous] = NewSide;

				// Find the prvious valid vertex to 'previous'
			//	int PrePrevious = Previous;
			//	do {
			//		PrePrevious = (PrePrevious + nVertices - 1)%nVertices;
			//	} 
			//	while (VertexRemoved[PrePrevious] == true);
					// Find the new angles for each vertex
					Angles[Next] = AngleBetweenLines(NewSide,Sides[Next]);
					Angles[Previous] = AngleBetweenLines(Sides[(Previous+nVertices-1)%nVertices], NewSide);

					VertexRemoved[CurrentVertex] = true;
					VerticesRemaining--;
				}
			}
		}
		CurrentVertex = (CurrentVertex+1)%nVertices;	// Cyclically increment the count	
	}

	// Remove allocated memory for arrays
	free(Angles);
	free(Vertices);
	free(Triangles);
	free(Sides);
	free(VertexRemoved);
}


// Fill in a horizontal triangle
void FillTriangle(xyCoordinate P1, xyCoordinate P2, xyCoordinate P3, double Z, double Thickness, double Permittivity)
{
	xyLine Sides[3];
	double Impedance;
	int xMin, xMax;
	int yMin, yMax;
	int zMin, zMax;

	// Set the impedance based upon the permittivity
	Impedance = IMPEDANCE_OF_FREE_SPACE/sqrt(Permittivity);

	if (Thickness < GridSpacing) {
		Thickness = GridSpacing;
	}

	// Setup the upper and lower z coordinate limits
	zMin = RoundUpwards((Z - Thickness/2)/GridSpacing);
	zMax = (int)((Z + Thickness/2)/GridSpacing);

	if (zMin < 0) {
		zMax -= zMin;
		zMin = 0;
	}
	if (zMax >= zSize) {
		zMin -= zMax - zSize + 1; 
		zMax = zSize-1;
	}
	zMin = PlaceWithinGridZ(zMin);
	zMax = PlaceWithinGridZ(zMax);

	// Arrange the coordinates in order of increasing x coordinate
	if (P1.X > P2.X) {
		xyCoordinate Swap = P1;
		P1 = P2;
		P2 = Swap;
	}
	if (P1.X > P3.X) {
		xyCoordinate Swap = P1;
		P1 = P3;
		P3 = Swap;
	}
	if (P2.X > P3.X) {
		xyCoordinate Swap = P2;
		P2 = P3;
		P3 = Swap;
	}

	Sides[0] = CoordinatesToLine(P1,P2);
	Sides[1] = CoordinatesToLine(P1,P3);
	Sides[2] = CoordinatesToLine(P2,P3);

	if (Sides[0].yCoeff != 0) {
		xMin = RoundUpwards(P1.X/GridSpacing);
		xMax = (int) (P2.X/GridSpacing);

		for (int x = xMin; x <= xMax; x++) {
			double y1 = YFromX(Sides[0],x*GridSpacing);
			double y2 = YFromX(Sides[1],x*GridSpacing);
			yMin = RoundUpwards(MIN(y1,y2)/GridSpacing);
			yMax = (int)(MAX(y1,y2)/GridSpacing);

			for (int y = yMin; y <= yMax; y++) {
				for (int z = zMin; z <= zMax; z++) {
					Grid[x][y][z].Z = Impedance;
				}
			}
		}
	}

	if (Sides[2].yCoeff != 0) {
		xMin = RoundUpwards(P2.X/GridSpacing);
		xMax = (int) (P3.X/GridSpacing);

		for (int x = xMin; x <= xMax; x++) {
			double y1 = YFromX(Sides[1],x*GridSpacing);
			double y2 = YFromX(Sides[2],x*GridSpacing);
			yMin = RoundUpwards(MIN(y1,y2)/GridSpacing);
			yMax = (int)(MAX(y1,y2)/GridSpacing);

			for (int y = yMin; y <= yMax; y++) {
				for (int z = zMin; z <= zMax; z++) {
					Grid[x][y][z].Z = Impedance;
				}
			}
		}
	}
}


// Ensure a point is within the grid in the x, y or z direction
int PlaceWithinGridX(int x)
{
	if (x < 0) x = 0;
	else if (x >= xSize) x = xSize - 1;

	return x;
}

int PlaceWithinGridY(int y)
{
	if (y < 0) y = 0;
	else if (y >= ySize) y = ySize - 1;

	return y;
}

int PlaceWithinGridZ(int z)
{
	if (z < 0) z = 0;
	else if (z >= zSize) z = zSize - 1;

	return z;
}


// Calculate the reflection and transmission coefficients of all nodes in the TLM grid based upon the node impedances
void CalculateReflectionTransmissionCoefficients(void)
{
	// Calculate reflection and transmission coefficients for each node
	for (int x = 0; x < xSize; x++) {
		for (int y = 0; y < ySize; y++) {
			for (int z = 0; z < zSize; z++) {
				// Positive x-direction
				if (x < xSize - 1) {
					Grid[x][y][z].Rxp = (Grid[x][y][z].Z - Grid[x+1][y][z].Z) / (Grid[x][y][z].Z + Grid[x+1][y][z].Z);
					Grid[x][y][z].Txp = 2 * Grid[x+1][y][z].Z / (Grid[x][y][z].Z + Grid[x+1][y][z].Z);
				}
				else {
					Grid[x][y][z].Rxp = 0.0;
					Grid[x][y][z].Txp = 1.0;
				}

				// Negative x-direction
				if (x > 0) {
					Grid[x][y][z].Rxn = (Grid[x][y][z].Z - Grid[x-1][y][z].Z) / (Grid[x][y][z].Z + Grid[x-1][y][z].Z);
					Grid[x][y][z].Txn = 2 * Grid[x-1][y][z].Z / (Grid[x][y][z].Z + Grid[x-1][y][z].Z);
				}
				else {
					Grid[x][y][z].Rxn = 0.0;
					Grid[x][y][z].Txn = 1.0;
				}

				// Positive y-direction
				if (y < ySize - 1) {
					Grid[x][y][z].Ryp = (Grid[x][y][z].Z - Grid[x][y+1][z].Z) / (Grid[x][y][z].Z + Grid[x][y+1][z].Z);
					Grid[x][y][z].Typ = 2 * Grid[x][y+1][z].Z / (Grid[x][y][z].Z + Grid[x][y+1][z].Z);
				}
				else {
					Grid[x][y][z].Ryp = 0.0;
					Grid[x][y][z].Typ = 1.0;
				}

				// Negative y-direction
				if (y > 0) {
					Grid[x][y][z].Ryn = (Grid[x][y][z].Z - Grid[x][y-1][z].Z) / (Grid[x][y][z].Z + Grid[x][y-1][z].Z);
					Grid[x][y][z].Tyn = 2 * Grid[x][y-1][z].Z / (Grid[x][y][z].Z + Grid[x][y-1][z].Z);
				}
				else {
					Grid[x][y][z].Ryn = 0.0;
					Grid[x][y][z].Tyn = 1.0;
				}

				// Positive z-direction
				if (z < zSize - 1) {
					Grid[x][y][z].Rzp = (Grid[x][y][z].Z - Grid[x][y][z+1].Z) / (Grid[x][y][z].Z + Grid[x][y][z+1].Z);
					Grid[x][y][z].Tzp = 2 * Grid[x][y][z+1].Z / (Grid[x][y][z].Z + Grid[x][y][z+1].Z);
				}
				else {
					Grid[x][y][z].Rzp = 0.0;
					Grid[x][y][z].Tzp = 1.0;
				}

				// Negative z-direction
				if (z > 0) {
					Grid[x][y][z].Rzn = (Grid[x][y][z].Z - Grid[x][y][z-1].Z) / (Grid[x][y][z].Z + Grid[x][y][z-1].Z);
					Grid[x][y][z].Tzn = 2 * Grid[x][y][z-1].Z / (Grid[x][y][z].Z + Grid[x][y][z-1].Z);
				}
				else {
					Grid[x][y][z].Rzn = 0.0;
					Grid[x][y][z].Tzn = 1.0;
				}
			}
		}
	}
}




