/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		25/10/2008
//	File:		TLMAlgorithm.cpp
//
/*********************************************************************************************/

#include "stdafx.h"
#include "TLMAlgorithm.h"
#include "TLMMaths.h"
#include "TLM.h"
#include "TLMSetup.h"
#include "TLMOutput.h"
#include "TLMScene.h"

// Event definitions
#define SCATTER_EVENT	WAIT_OBJECT_0
#define CONNECT_EVENT	WAIT_OBJECT_0+1
#define END_EVENT		WAIT_OBJECT_0+2


// Type Definitions

typedef struct {
				int X;
				int Y;
				int Z;
				} SetIndex_t;

typedef struct {
				int xMin;
				int xMax;
				int yMin;
				int yMax;
				int zMin;
				int zMax;
				} SetData_t;


// Global variables
static int ***ActiveJunctions;
double AbsoluteThreshold;

static ActiveNode ****ActiveSet;
static ActiveNode *****NodeAdditions;		// 6 element array [Xn, Xp, Yn, Yp, Zn, Zp]
static HANDLE *hReadyEvent;
static HANDLE *hScatterEvent;
static HANDLE *hConnectEvent;
static HANDLE *hEndEvent;
static HANDLE hMutex;
static SetIndex_t *SetOrder;
static bool *Taken;
static SetIndex_t MaxSetIndex;
static SetData_t ***SetData;
static HANDLE *hWorkerThreads;
static DWORD *dwWorkerThreadIDs;

extern Node ***Grid;
extern int xSize, ySize, zSize;
extern Source ImpulseSource;
extern InputFlags InputData;

extern double Frequency;
extern double MaxPathLoss;
extern double RelativeThreshold;
extern double GridSpacing;
extern int Threads;
extern int Sets;


// Function prototypes
DWORD WINAPI WorkerThread(LPVOID *lpParam);
void Scatter(SetIndex_t *Index);
void Connect(SetIndex_t *Index);
void EvaluateSource(int Iteration);
void CalculateBoundary(void);
ActiveNode *AddJunctionToSet(int x, int y, int z, bool Active);
ActiveNode *RemoveJunctionFromSet(int x, int y, int z, ActiveNode *InactiveNode);
void CopyNodeAdditions(SetIndex_t *Index);
void CalculateSectionIndices(void);
void AllocateResources(void);
void FreeResources(void);
void CalculateInitialBoundaries(void);
int CompareActiveJunctions(const void *Set1, const void *Set2);


// Top level loop for TLM algorithm
void MainLoop(void)
{
	int nIterations = 0;
	int n = 0;
	bool Empty = false;

	// Calculate the absolute threshold from the path loss
	AbsoluteThreshold = SQUARE(4*M_PI*GridSpacing/KAPPA*Frequency/SPEED_OF_LIGHT)*pow(10, MaxPathLoss/10.0);
	RelativeThreshold *= RelativeThreshold;

	// Calculate the boundaries
	CalculateSectionIndices();
	AllocateResources();
	CalculateInitialBoundaries();

	Sets = MaxSetIndex.X * MaxSetIndex.Y * MaxSetIndex.Z;

	// Evaluate source output
	if (nIterations < ImpulseSource.Duration) {
		EvaluateSource(nIterations);
	}

	// Allocate memory for the ready event array and worker threads
	SetOrder = (SetIndex_t*)malloc(Sets*sizeof(SetIndex_t));
	Taken = (bool*)malloc(Sets*sizeof(bool));
	for (int i=0; i<MaxSetIndex.X; i++) {
		for (int j=0; j<MaxSetIndex.Y; j++) {
			for (int k=0; k<MaxSetIndex.Z; k++) {
				SetOrder[n].X = i;
				SetOrder[n].Y = j;
				SetOrder[n].Z = k;
				Taken[n] = false;
				n++;
			}
		}
	}

	// Wait for the workers to become ready
	WaitForMultipleObjects(Threads, hReadyEvent, true, INFINITE);

	// Repeat the algorithm while the active set is not empty
	while (Empty == false) {

		// Tell the worker threads to scatter
		for (int i=0; i<Threads; i++) {
			SetEvent(hScatterEvent[i]);
		}

		// Wait for the workers to acknowledge
		WaitForMultipleObjects(Threads, hReadyEvent, true, INFINITE);

		// Reset all of the taken flags
		for (int i=0; i<Sets; i++) {
			Taken[i] = false;
		}

		// Tell the worker threads to connect
		for (int i=0; i<Threads; i++) {
			SetEvent(hConnectEvent[i]);
		}

		// Wait for the workers to acknowledge
		WaitForMultipleObjects(Threads, hReadyEvent, true, INFINITE);

		// Reset all of the taken flags
		for (int i=0; i<Sets; i++) {
			Taken[i] = false;
		}

		// Sort the set order in increasing number of active junctions
		qsort(SetOrder, Sets, sizeof(SetIndex_t), CompareActiveJunctions);

	/*	for (int i=0; i<Sets; i++) {
			printf("Number %d:\t(%d,%d,%d):\t%d active junctions\n", i+1, SetOrder[i].X, SetOrder[i].Y, SetOrder[i].Z, ActiveJunctions[SetOrder[i].X][SetOrder[i].Y][SetOrder[i].Z]);
		}*/

		// Increment the number of iterations completed
		nIterations++;

		// Check for empty active sets
		Empty = true;
		for (int i=0; i<MaxSetIndex.X; i++) {
			for (int j=0; j<MaxSetIndex.Y; j++) {
				for (int k=0; k<MaxSetIndex.Z; k++) {
					if (ActiveSet[i][j][k] != NULL) {
						Empty = false;
					}
				}
			}
		}

		printf("Completed %d iterations\n", nIterations);
		for (int i=0; i<MaxSetIndex.X; i++) {
			for (int j=0; j<MaxSetIndex.Y; j++) {
				for (int k=0; k<MaxSetIndex.Z; k++) {
					printf("\tSet (%d,%d,%d):\t%d active junctions\n", i+1, j+1, k+1, ActiveJunctions[i][j][k]);
				}
			}
		}
	}

	// Tell the worker threads to finish
	for (int i=0; i<Threads; i++) {
		SetEvent(hEndEvent[i]);
	}

	// Wait for the worker threads to terminate
	WaitForMultipleObjects(Threads, hWorkerThreads, true, INFINITE);

	// Free memory allocated to the synchronisation
	FreeResources();

	printf("Algorithm complete, took %d iterations\n", nIterations);
}


// Secondary Thread Function
DWORD WINAPI WorkerThread(LPVOID *lpParam)
{
	int ThreadNumber = (int)(long long)lpParam;
	HANDLE hEventArray[3];
	DWORD EventBuffer;
	int SetIndex;

	hEventArray[0] = hScatterEvent[ThreadNumber];
	hEventArray[1] = hConnectEvent[ThreadNumber];
	hEventArray[2] = hEndEvent[ThreadNumber];

	// Tell the main thread the worker is ready
	SetEvent(hReadyEvent[ThreadNumber]);

	while (1) {
		// Wait until a message has been posted
		EventBuffer = WaitForMultipleObjects(3, hEventArray, false, INFINITE);

		switch (EventBuffer) {
			case SCATTER_EVENT:
				SetIndex = 0;
				while (SetIndex < Sets) {
					WaitForSingleObject(hMutex, INFINITE);					
					while (Taken[SetIndex] == true && SetIndex < Sets) {
						SetIndex++;
					}
					if (SetIndex<Sets) {
						Taken[SetIndex] = true;
					}
					ReleaseMutex(hMutex);
					if (SetIndex<Sets) {
						Scatter(&SetOrder[SetIndex]);
					}
				}
				break;

			case CONNECT_EVENT:
				SetIndex = 0;
				while (SetIndex < Sets) {
					WaitForSingleObject(hMutex, INFINITE);					
					while (Taken[SetIndex] == true && SetIndex < Sets) {
						SetIndex++;
					}
					if (SetIndex<Sets) {
						Taken[SetIndex] = true;
					}
					ReleaseMutex(hMutex);
					if (SetIndex<Sets) {
						CopyNodeAdditions(&SetOrder[SetIndex]);
						Connect(&SetOrder[SetIndex]);
					}
				}
				break;

			case END_EVENT:
				ExitThread(0);
				break;

			default:
				printf("Thread %d received error", ThreadNumber);
				Sleep(2000);
				exit(1);
		}
		SetEvent(hReadyEvent[ThreadNumber]);
	}
}


// Single iteration of the TLM algorithm scatter sequence
void Scatter(SetIndex_t *Index) 
{
	double Value;			// Temporary node value
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *NewNode;
	int x, y, z;
	int xIndex = Index->X;
	int yIndex = Index->Y;
	int zIndex = Index->Z;
	SetData_t *Data = &SetData[xIndex][yIndex][zIndex];
	int xMin = Data->xMin;
	int xMax = Data->xMax;
	int yMin = Data->yMin;
	int yMax = Data->yMax;
	int zMin = Data->zMin;
	int zMax = Data->zMax;

	// Setup the current node pointer
	CurrentNode = ActiveSet[xIndex][yIndex][zIndex];

	// Scatter phase, compute the junction outputs for all of the junctions in the active set
	while (CurrentNode != NULL) {
		x = CurrentNode->X;
		y = CurrentNode->Y;
		z = CurrentNode->Z;

		NodeReference = &Grid[x][y][z];
		Value = NodeReference->V/3;
		NodeReference->VxpOut = Value - NodeReference->VxpIn;
		NodeReference->VxnOut = Value - NodeReference->VxnIn;
		NodeReference->VypOut = Value - NodeReference->VypIn;
		NodeReference->VynOut = Value - NodeReference->VynIn;
		NodeReference->VzpOut = Value - NodeReference->VzpIn;
		NodeReference->VznOut = Value - NodeReference->VznIn;

		// Check whether adjacent nodes need to be added to the active junction set
		// Positive x direction
		if (x < (xSize-1)) {
			if (x == xMax) {
				if (Grid[x+1][y][z].Active == false && Grid[x+1][y][z].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x+1,y,z, false);
					NewNode->NextActiveNode = NodeAdditions[xIndex+1][yIndex][zIndex][0];
					NodeAdditions[xIndex+1][yIndex][zIndex][0] = NewNode;
				}
			}
			else {
				if (Grid[x+1][y][z].Active == false && Grid[x+1][y][z].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x+1,y,z, true);
					NewNode->NextActiveNode = ActiveSet[xIndex][yIndex][zIndex];
					ActiveSet[xIndex][yIndex][zIndex] = NewNode;
					ActiveJunctions[xIndex][yIndex][zIndex]++;
				}
			}
		
		}

		// Negative x direction
		if (x > 0) {
			if (x == xMin) {
				if (Grid[x-1][y][z].Active == false && Grid[x-1][y][z].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x-1,y,z, false);
					NewNode->NextActiveNode = NodeAdditions[xIndex-1][yIndex][zIndex][1];
					NodeAdditions[xIndex-1][yIndex][zIndex][1] = NewNode;
				}
			}
			else {
				if (Grid[x-1][y][z].Active == false && Grid[x-1][y][z].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x-1,y,z, true);
					NewNode->NextActiveNode = ActiveSet[xIndex][yIndex][zIndex];
					ActiveSet[xIndex][yIndex][zIndex] = NewNode;
					ActiveJunctions[xIndex][yIndex][zIndex]++;
				}
			}
		}

		// Positive y direction
		if (y < (ySize - 1)) {
			if (y == yMax) {
				if (Grid[x][y+1][z].Active == false && Grid[x][y+1][z].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x,y+1,z,false);
					NewNode->NextActiveNode = NodeAdditions[xIndex][yIndex+1][zIndex][2];
					NodeAdditions[xIndex][yIndex+1][zIndex][2] = NewNode;
				}				
			}
			else {
				if (Grid[x][y+1][z].Active == false && Grid[x][y+1][z].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x,y+1,z, true);
					NewNode->NextActiveNode = ActiveSet[xIndex][yIndex][zIndex];
					ActiveSet[xIndex][yIndex][zIndex] = NewNode;
					ActiveJunctions[xIndex][yIndex][zIndex]++;
				}
			}
		}

		// Negative y direction
		if (y > 0) {
			if (y == yMin) {
				if (Grid[x][y-1][z].Active == false && Grid[x][y-1][z].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x,y-1,z, false);
					NewNode->NextActiveNode = NodeAdditions[xIndex][yIndex-1][zIndex][3];
					NodeAdditions[xIndex][yIndex-1][zIndex][3] = NewNode;
				}
			}
			else {
				if (Grid[x][y-1][z].Active == false && Grid[x][y-1][z].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x,y-1,z, true);
					NewNode->NextActiveNode = ActiveSet[xIndex][yIndex][zIndex];
					ActiveSet[xIndex][yIndex][zIndex] = NewNode;
					ActiveJunctions[xIndex][yIndex][zIndex]++;
				}
			}
		}
		// Positive z direction
		if (z < (zSize - 1)) {
			if (z == zMax) {
				if (Grid[x][y][z+1].Active == false && Grid[x][y][z+1].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x,y,z+1, false);
					NewNode->NextActiveNode = NodeAdditions[xIndex][yIndex][zIndex+1][4];
					NodeAdditions[xIndex][yIndex][zIndex+1][4] = NewNode;
				}
			}
			else {
				if (Grid[x][y][z+1].Active == false && Grid[x][y][z+1].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x,y,z+1, true);
					NewNode->NextActiveNode = ActiveSet[xIndex][yIndex][zIndex];
					ActiveSet[xIndex][yIndex][zIndex] = NewNode;
					ActiveJunctions[xIndex][yIndex][zIndex]++;
				}
			}
		}
		// Negative y direction
		if (z > 0) {
			if (z == zMin) {
				if (Grid[x][y][z-1].Active == false && Grid[x][y][z-1].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x,y,z-1, false);
					NewNode->NextActiveNode = NodeAdditions[xIndex][yIndex][zIndex-1][5];
					NodeAdditions[xIndex][yIndex][zIndex-1][5] = NewNode;
				}
			}
			else {
				if (Grid[x][y][z-1].Active == false && Grid[x][y][z-1].PropagateFlag == true) {
					NewNode = AddJunctionToSet(x,y,z-1, true);
					NewNode->NextActiveNode = ActiveSet[xIndex][yIndex][zIndex];
					ActiveSet[xIndex][yIndex][zIndex] = NewNode;
					ActiveJunctions[xIndex][yIndex][zIndex]++;
				}
			}
		}
		// Get the next node in the active set
		CurrentNode = CurrentNode->NextActiveNode;
	}
}


// Single iteration of the TLM algorithm connent sequence
void Connect(SetIndex_t *Index) 
{
	double Value;			// Temporary node value
	double AvgEnergy;		// Average energy over two iterations
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *PreviousNode;
	int xIndex = Index->X;
	int yIndex = Index->Y;
	int zIndex = Index->Z;

	int x, y, z;

	// Connect phase, compute the junction inputs for all of the junctions in the active set
	PreviousNode = NULL;
	CurrentNode = ActiveSet[xIndex][yIndex][zIndex];

	while (CurrentNode != NULL) {

		// Get local copies of the position variables and the node pointer
		x = CurrentNode->X;
		y = CurrentNode->Y;
		z = CurrentNode->Z;
		NodeReference = &Grid[x][y][z];

		// Check if the node is a material or grid boundary, if so then incorporate transmission and reflection coefficients
		if (Grid[x][y][z].RT == NULL) {
			if (x < (xSize-1)) {
				NodeReference->VxpIn = Grid[x+1][y][z].VxnOut;
			}
			if (x > 0) {
				NodeReference->VxnIn = Grid[x-1][y][z].VxpOut;
			}
			else {
				NodeReference->VxnIn = 0;
			}
			if (y < (ySize-1)) {
				NodeReference->VypIn = Grid[x][y+1][z].VynOut;
			}
			else {
				NodeReference->VypIn = 0;
			}
			if (y > 0) {
				NodeReference->VynIn = Grid[x][y-1][z].VypOut;
			}
			else {
				NodeReference->VynIn = 0;
			}
			if (z < (zSize-1)) {
				NodeReference->VzpIn = Grid[x][y][z+1].VznOut;
			}
			else {
				NodeReference->VzpIn = 0;
			}
			if (z > 0) {
				NodeReference->VznIn = Grid[x][y][z-1].VzpOut;
			}
			else {
				NodeReference->VznIn = 0;
			}
		}
		else {
			RTCoeffs *RT = NodeReference->RT;
			// Positive x-direction
			NodeReference->VxpIn = RT->Rxp*NodeReference->VxpOut;
			if (x < (xSize-1)) {
				NodeReference->VxpIn += Grid[x+1][y][z].VxnOut * RT->Txp;
			}

			// Negative x-direction
			NodeReference->VxnIn = RT->Rxn*NodeReference->VxnOut;
			if (x > 0) {
				NodeReference->VxnIn += Grid[x-1][y][z].VxpOut * RT->Txn;
			}

			// Positive y-direction
			NodeReference->VypIn = RT->Ryp*NodeReference->VypOut;
			if (y < (ySize-1)) {
				NodeReference->VypIn += Grid[x][y+1][z].VynOut * RT->Typ;
			}

			// Negative y-direction
			NodeReference->VynIn = RT->Ryn*NodeReference->VynOut;
			if (y > 0) {
				NodeReference->VynIn += Grid[x][y-1][z].VypOut * RT->Tyn;
			}

			// Positive z-direction
			NodeReference->VzpIn = RT->Rzp*NodeReference->VzpOut;
			if (z < (zSize-1)) {
				NodeReference->VzpIn += Grid[x][y][z+1].VznOut * RT->Tzp;
			}

			// Negative z-direction
			NodeReference->VznIn = RT->Rzn*NodeReference->VznOut;
			if (z > 0) {
				NodeReference->VznIn += Grid[x][y][z-1].VzpOut * RT->Tzn;
			}
		}

		// Compute the state of the node
		Value = NodeReference->VxpIn + 
				NodeReference->VxnIn +
				NodeReference->VypIn +
				NodeReference->VynIn +
				NodeReference->VzpIn +
				NodeReference->VznIn;

		// Compute the average energy over the previous two node voltages
		AvgEnergy = SQUARE(Value) + SQUARE(Grid[x][y][z].V);
		
		// Add instantaneous energy to the total energy at this node
		Grid[x][y][z].Epulse += SQUARE(Value);

		// Update the maximum pulse energy if necessary
		if (Grid[x][y][z].Epulse > Grid[x][y][z].Emax) {
			Grid[x][y][z].Emax = Grid[x][y][z].Epulse;
		}
		
		// Assign to the node
		Grid[x][y][z].V = Value;

		if (AvgEnergy < AbsoluteThreshold || AvgEnergy < NodeReference->Emax*RelativeThreshold) {
			CurrentNode = RemoveJunctionFromSet(x,y,z,CurrentNode);
			ActiveJunctions[xIndex][yIndex][zIndex]--;
			if (PreviousNode != NULL) {
				PreviousNode->NextActiveNode = CurrentNode;
			}
			else {
				ActiveSet[xIndex][yIndex][zIndex] = CurrentNode;
			}
		}
		else {
			// Get the next active node from the list
			PreviousNode = CurrentNode;
			CurrentNode = CurrentNode->NextActiveNode;
		}
	}
}


// Evaluate the source input to the grid
void EvaluateSource(int Iteration)
{
	double V;

	V = Grid[ImpulseSource.X][ImpulseSource.Y][ImpulseSource.Z].V;

	switch (ImpulseSource.Type) {
		case IMPULSE:
			V += 1;
			break;
		case GAUSSIAN:
			V += exp(10*-SQUARE(((double)Iteration-ImpulseSource.Duration/2)/ImpulseSource.Duration));
			break;
		case RAISED_COSINE:
			V += 0.5*(1+cos(2*M_PI*((double)(Iteration+1)/(ImpulseSource.Duration)-0.5)));
			break;
	}

	// Compute the average energy over the previous and current iterations
	Grid[ImpulseSource.X][ImpulseSource.Y][ImpulseSource.Z].V = V;

	Grid[ImpulseSource.X][ImpulseSource.Y][ImpulseSource.Z].Epulse += SQUARE(V);

	// Update the maximum energy if greater
	if (Grid[ImpulseSource.X][ImpulseSource.Y][ImpulseSource.Z].Epulse > Grid[ImpulseSource.X][ImpulseSource.Y][ImpulseSource.Z].Emax) {
		Grid[ImpulseSource.X][ImpulseSource.Y][ImpulseSource.Z].Emax = Grid[ImpulseSource.X][ImpulseSource.Y][ImpulseSource.Z].Epulse;
	}
}


// Calculate the position of the dynamic boundary
void CalculateBoundary(void)
{
	/*int OldBoundary;// = xMax[0];

	if (ActiveJunctions[0] > ActiveJunctions[1]) {
		xMax[0] = PlaceWithinGridX(RoundToNearest(xMax[0] + xMax[0] * double(ActiveJunctions[1]-ActiveJunctions[0])/2.0/double(ActiveJunctions[0]+ActiveJunctions[1])));
	}
	else {
		xMax[0] = PlaceWithinGridX(RoundToNearest(xMax[0] + (xSize-1-xMax[0]) * double(ActiveJunctions[1]-ActiveJunctions[0])/2.0/double(ActiveJunctions[0]+ActiveJunctions[1])));
	}
	xMin[1] = xMax[0]+1;
	
	// Remove nodes from the smaller list that are no longer in the correct active set
	if (OldBoundary < xMax[0]) {
		CorrectActiveSet(1);
	}
	else if (OldBoundary > xMax[0]) {
		CorrectActiveSet(0);
	}*/
}


// Return a newly allocated ActiveNode structure with the coordinates given
ActiveNode *AddJunctionToSet(int x, int y, int z, bool Active)
{
	ActiveNode *NewNode;

	NewNode = (ActiveNode*)malloc(sizeof(ActiveNode)); 


	NewNode->X = x;
	NewNode->Y = y;
	NewNode->Z = z;
	NewNode->NextActiveNode = NULL;
	if (Active == true) {
		Grid[x][y][z].Active = true;
	}

	return NewNode;
}


// Remove a junction from the active set and free memory allocated to it, returns the a pointer to the rest of the list which should be appended to the first half
ActiveNode *RemoveJunctionFromSet(int x, int y, int z, ActiveNode *InactiveNode)
{
	ActiveNode *NextNode;
	
	NextNode = InactiveNode->NextActiveNode;
	free(InactiveNode);
	Grid[x][y][z].Active = false;
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
	Grid[x][y][z].Epulse = 0;

	return NextNode;
}


// Add boundary nodes to the active list, return the new list
void CopyNodeAdditions(SetIndex_t *Index)
{
	ActiveNode *TempNode, *CurrentNode, *PreviousNode;
	int xIndex = Index->X;
	int yIndex = Index->Y;
	int zIndex = Index->Z;

	for (int i=0; i<6; i++) {

		PreviousNode = NULL;
		CurrentNode = NodeAdditions[xIndex][yIndex][zIndex][i];
		while (CurrentNode != NULL) {
			if (Grid[CurrentNode->X][CurrentNode->Y][CurrentNode->Z].Active == true) {
				// Junction already added to active set, remove the node from the active set
				TempNode = CurrentNode;
				CurrentNode = CurrentNode->NextActiveNode;
				free(TempNode);
				if (PreviousNode != NULL) {
					PreviousNode->NextActiveNode = CurrentNode;
				}
				else {
					NodeAdditions[xIndex][yIndex][zIndex][i] = CurrentNode;
				}
			}
			else {
				ActiveJunctions[xIndex][yIndex][zIndex]++;
				Grid[CurrentNode->X][CurrentNode->Y][CurrentNode->Z].Active = true;
				// Get the next node in the list
				PreviousNode = CurrentNode;
				CurrentNode = CurrentNode->NextActiveNode;
			}
		}

		// Add the list to the head of the main list
		if (NodeAdditions[xIndex][yIndex][zIndex][i] != NULL) {
			if (PreviousNode != NULL) {
				PreviousNode->NextActiveNode = ActiveSet[xIndex][yIndex][zIndex];
			}
			else {
				NodeAdditions[xIndex][yIndex][zIndex][i]->NextActiveNode = ActiveSet[xIndex][yIndex][zIndex];
			}
			ActiveSet[xIndex][yIndex][zIndex] = NodeAdditions[xIndex][yIndex][zIndex][i];
			NodeAdditions[xIndex][yIndex][zIndex][i] = NULL;
		}
	}
}




void CalculateSectionIndices(void)
{
	int nSections = 1;
	int MaxSizes[3] = {1,1,1};
	int MaxSections = 1;

	for (int i=1; i<=Sets; i++) {
		for (int j=1; j<=Sets/i; j++) {
			for (int k=1; k<= Sets/i/j; k++) {
				nSections = i*j*k;
				if (nSections > MaxSections || (nSections == MaxSections && i+j+k < MaxSizes[0]+MaxSizes[1]+MaxSizes[2])) {
					MaxSections = nSections;
					if (i>=j) {
						if (i>=k) {
							MaxSizes[0] = i;
							if (j>=k) {
								MaxSizes[1] = j;
								MaxSizes[2] = k;
							}
							else {
								MaxSizes[1] = k;
								MaxSizes[2] = j;
							}
						}
						else {
							MaxSizes[0] = k;
							MaxSizes[1] = i;
							MaxSizes[2] = j;							
						}
					}
					else {
						if (i<k) {
							MaxSizes[2] = i;
							if (j>=k) {
								MaxSizes[0] = j;
								MaxSizes[1] = k;
							}
							else {
								MaxSizes[0] = k;
								MaxSizes[1] = j;
							}
						}
						else {
							MaxSizes[0] = j;
							MaxSizes[1] = i;
							MaxSizes[2] = k;
						}
					}
				}
			}
		}
	}

	if (xSize >= ySize) {
		if (xSize >= zSize) {
			MaxSetIndex.X = MaxSizes[0];
			if (ySize >= zSize) {
				MaxSetIndex.Y = MaxSizes[1];
				MaxSetIndex.Z = MaxSizes[2];
			}
			else {
				MaxSetIndex.Z = MaxSizes[1];
				MaxSetIndex.Y = MaxSizes[2];
			}
		}
		else {
			MaxSetIndex.Z = MaxSizes[0];
			MaxSetIndex.X = MaxSizes[1];
			MaxSetIndex.Y = MaxSizes[2];							
		}
	}
	else {
		if (xSize < zSize) {
			MaxSetIndex.X = MaxSizes[2];
			if (ySize >= zSize) {
				MaxSetIndex.Y = MaxSizes[0];
				MaxSetIndex.Z = MaxSizes[1];
			}
			else {
				MaxSetIndex.Z = MaxSizes[0];
				MaxSetIndex.Y = MaxSizes[1];
			}
		}
		else {
			MaxSetIndex.Y = MaxSizes[0];
			MaxSetIndex.X = MaxSizes[1];
			MaxSetIndex.Z = MaxSizes[2];
		}
	}
}


// Allocate multithreading resources
void AllocateResources(void)
{
	int nSets = (MaxSetIndex.X)*(MaxSetIndex.Y)*(MaxSetIndex.Z);

	hMutex = CreateMutex(NULL, FALSE, NULL);

	hWorkerThreads = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	dwWorkerThreadIDs = (DWORD*)malloc(Threads*sizeof(DWORD));
	hReadyEvent = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	hScatterEvent = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	hConnectEvent = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	hEndEvent = (HANDLE*)malloc(Threads*sizeof(HANDLE));

	ActiveJunctions = (int***)malloc(MaxSetIndex.X*sizeof(int**));
	ActiveSet = (ActiveNode****)malloc(MaxSetIndex.X*sizeof(ActiveNode***));
	NodeAdditions = (ActiveNode*****)malloc(MaxSetIndex.X*sizeof(ActiveNode****));
	SetData = (SetData_t***)malloc(MaxSetIndex.X*sizeof(SetData_t**));

	for (int i=0; i<MaxSetIndex.X; i++) {
		SetData[i] = (SetData_t**)malloc(MaxSetIndex.Y*sizeof(SetData_t*));
		ActiveJunctions[i] = (int**)malloc(MaxSetIndex.Y*sizeof(int*));
		ActiveSet[i] = (ActiveNode***)malloc(MaxSetIndex.Y*sizeof(ActiveNode**));
		NodeAdditions[i] = (ActiveNode****)malloc(MaxSetIndex.Y*sizeof(ActiveNode***));

		for (int j=0; j<MaxSetIndex.Y; j++) {		
			SetData[i][j] = (SetData_t*)malloc(MaxSetIndex.Z*sizeof(SetData_t));
			ActiveJunctions[i][j] = (int*)malloc(MaxSetIndex.Z*sizeof(int));
			ActiveSet[i][j] = (ActiveNode**)malloc(MaxSetIndex.Z*sizeof(ActiveNode*));
			NodeAdditions[i][j] = (ActiveNode***)malloc(MaxSetIndex.Z*sizeof(ActiveNode**));

			for (int k=0; k<MaxSetIndex.Z; k++) {
				// Initialise the active junctions
				ActiveJunctions[i][j][k] = 0;

				// Initialise the list heads
				ActiveSet[i][j][k] = NULL;
				NodeAdditions[i][j][k] = (ActiveNode**)malloc(6*sizeof(ActiveNode*));
				NodeAdditions[i][j][k][0] = NULL;
				NodeAdditions[i][j][k][1] = NULL;
				NodeAdditions[i][j][k][2] = NULL;
				NodeAdditions[i][j][k][3] = NULL;
				NodeAdditions[i][j][k][4] = NULL;
				NodeAdditions[i][j][k][5] = NULL;
			}
		}
	}

	for (int i=0; i<Threads; i++) {
		// Create the events
		hReadyEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		hScatterEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		hConnectEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		hEndEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);

		hWorkerThreads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, (LPVOID)(long long)i, 0, &dwWorkerThreadIDs[i]);
		if (hWorkerThreads[i] == NULL) {
			printf("Worker thread for block %d could not be started\n", i+1);
			exit(1);
		}
		else {
			printf("Worker thread %d started\n", i+1);
		}
	}
}


// Removed allocated memory for thread control
void FreeResources(void)
{
	for (int i=0; i<MaxSetIndex.X; i++) {
		for (int j=0; j<MaxSetIndex.Y; j++) {
			free(ActiveJunctions[i][j]);
			free(ActiveSet[i][j]);
			free(NodeAdditions[i][j]);
			free(SetData[i][j]);
		}
		free(ActiveJunctions[i]);
		free(ActiveSet[i]);
		free(NodeAdditions[i]);
		free(SetData[i]);
	}
	free(ActiveJunctions);
	free(ActiveSet);
	free(NodeAdditions);
	free(hReadyEvent);
	free(hScatterEvent);
	free(hConnectEvent);
	free(hEndEvent);
	free(SetData);
}


void CalculateInitialBoundaries(void)
{
	for (int i=0; i<MaxSetIndex.X; i++) {
		for (int j=0; j<MaxSetIndex.Y; j++) {
			for (int k=0; k<MaxSetIndex.Z; k++) {
				SetData[i][j][k].xMin = RoundToNearest(i*xSize/MaxSetIndex.X);
				SetData[i][j][k].xMax = RoundToNearest((i+1)*xSize/MaxSetIndex.X-1);
				SetData[i][j][k].yMin = RoundToNearest(j*ySize/MaxSetIndex.Y);
				SetData[i][j][k].yMax = RoundToNearest((j+1)*ySize/MaxSetIndex.Y-1);
				SetData[i][j][k].zMin = RoundToNearest(k*zSize/MaxSetIndex.Z);
				SetData[i][j][k].zMax = RoundToNearest((k+1)*zSize/MaxSetIndex.Z-1);
				if (ImpulseSource.X >= SetData[i][j][k].xMin && ImpulseSource.X <= SetData[i][j][k].xMax &&
					ImpulseSource.Y >= SetData[i][j][k].yMin && ImpulseSource.Y <= SetData[i][j][k].yMax &&
					ImpulseSource.Z >= SetData[i][j][k].zMin && ImpulseSource.Z <= SetData[i][j][k].zMax) 
				{
					ActiveJunctions[i][j][k] = 1;
					ActiveSet[i][j][k] = AddJunctionToSet(ImpulseSource.X, ImpulseSource.Y, ImpulseSource.Z, true);
				}
			}
		}
	}
}


// Compare the size of two active junctions (for quick sort algorithm)
int CompareActiveJunctions(const void *Set1, const void *Set2)
{
	return (ActiveJunctions[(*(SetIndex_t*)Set2).X][(*(SetIndex_t*)Set2).Y][(*(SetIndex_t*)Set2).Z] - ActiveJunctions[(*(SetIndex_t*)Set1).X][(*(SetIndex_t*)Set1).Y][(*(SetIndex_t*)Set1).Z]);
}
