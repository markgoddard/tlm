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


// Type Definitions
typedef volatile enum {
						NO_MSG,
						READY,
						SCATTER,
						CONNECT,
						END
						} Msg_t;

typedef struct {
				int X;
				int Y;
				int Z;
				} ThreadIndex_t;

typedef struct {
				ThreadIndex_t Index;
				int xMin;
				int xMax;
				int yMin;
				int yMax;
				int zMin;
				int zMax;
				} ThreadData_t;


// Global variables
static int ***ActiveJunctions;
double AbsoluteThreshold;

static ActiveNode ****ActiveSet;
static ActiveNode *****NodeAdditions;		// 6 element array [Xn, Xp, Yn, Yp, Zn, Zp]
static HANDLE ***hMutexA;
static HANDLE ***hMutexB;
static HANDLE ***hMutexC;
static HANDLE ***hMutexMsg;
static Msg_t ***Msg;
static ThreadData_t ***ThreadData;
static ThreadIndex_t MaxThreadIndex;
static HANDLE ***hWorkerThreads;
static DWORD ***dwWorkerThreadIDs;

extern Node ***Grid;
extern int xSize, ySize, zSize;
extern Source ImpulseSource;
extern InputFlags InputData;

extern double Frequency;
extern double MaxPathLoss;
extern double RelativeThreshold;
extern double GridSpacing;
extern int Threads;


// Function prototypes
DWORD WINAPI WorkerThread(LPVOID *lpParam);
void Scatter(ThreadData_t *Data);
void Connect(ThreadData_t *Data);
void EvaluateSource(int Iteration);
void CalculateBoundary(void);
ActiveNode *AddJunctionToSet(int x, int y, int z, bool Active);
ActiveNode *RemoveJunctionFromSet(int x, int y, int z, ActiveNode *InactiveNode);
void CopyNodeAdditions(ThreadData_t *Data);
void CorrectActiveSet(int Set1);
void CalculateSectionIndices(void);
void AllocateResources(void);
void FreeResources(void);
void CalculateInitialBoundaries(void);


// Top level loop for TLM algorithm
void MainLoop(void)
{
	int nIterations = 0;
	int n = 0;
	int nThreads;
	Msg_t ***MsgBuffer;
	bool Success;
	bool Empty = false;
	HANDLE *hMutexArrayA;
	HANDLE *hMutexArrayB;
	HANDLE *hMutexArrayC;
	HANDLE *hWorkerThreadArray;

	// Calculate the absolute threshold from the path loss
	AbsoluteThreshold = SQUARE(4*M_PI*GridSpacing/KAPPA*Frequency/SPEED_OF_LIGHT)*pow(10, MaxPathLoss/10.0);
	RelativeThreshold *= RelativeThreshold;

	// Calculate the boundaries
	CalculateSectionIndices();
	AllocateResources();
	CalculateInitialBoundaries();

	// Allocate memory for the message buffer and initialise to no message
	MsgBuffer = (Msg_t***)malloc(MaxThreadIndex.X*sizeof(Msg_t**));
	for (int i=0; i<MaxThreadIndex.X; i++) {
		MsgBuffer[i] = (Msg_t**)malloc(MaxThreadIndex.Y*sizeof(Msg_t*));
		for (int j=0; j<MaxThreadIndex.Y; j++) {
			MsgBuffer[i][j] = (Msg_t*)malloc(MaxThreadIndex.Z*sizeof(Msg_t));
			for (int k=0; k<MaxThreadIndex.Z; k++) {
				MsgBuffer[i][j][k] = NO_MSG;
			}
		}
	}
	nThreads = MaxThreadIndex.X * MaxThreadIndex.Y * MaxThreadIndex.Z;

	// Evaluate source output
	if (nIterations < ImpulseSource.Duration) {
		EvaluateSource(nIterations);
	}

	// Wait for the workers to become ready
	do {
		Sleep(1);
		Success = true;
		for (int i=0; i<MaxThreadIndex.X; i++) {
			for (int j=0; j<MaxThreadIndex.Y; j++) {
				for (int k=0; k<MaxThreadIndex.Z; k++) {
					WaitForSingleObject(hMutexMsg[i][j][k], INFINITE);
					if (Msg[i][j][k] != READY) {
						Success = false;
					}
					ReleaseMutex(hMutexMsg[i][j][k]);
				}
			}
		}
	}
	while (Success == false);

	// Allocate memory for the mutex arrays
	hMutexArrayA = (HANDLE*)malloc(nThreads * sizeof(HANDLE));
	hMutexArrayB = (HANDLE*)malloc(nThreads * sizeof(HANDLE));
	hMutexArrayC = (HANDLE*)malloc(nThreads * sizeof(HANDLE));
	hWorkerThreadArray = (HANDLE*)malloc(nThreads * sizeof(HANDLE));
	for (int i=0; i<MaxThreadIndex.X; i++) {
		for (int j=0; j<MaxThreadIndex.Y; j++) {
			for (int k=0; k<MaxThreadIndex.Z; k++) {
				hMutexArrayA[n] = hMutexA[i][j][k];
				hMutexArrayB[n] = hMutexB[i][j][k];
				hMutexArrayC[n] = hMutexC[i][j][k];
				hWorkerThreadArray[n] = hWorkerThreads[i][j][k];
				n++;
			}
		}
	}

	// Repeat the algorithm while the active set is not empty
	while (Empty == false) {

		WaitForMultipleObjects(nThreads, hMutexArrayC, true, INFINITE);
		
		// Tell the worker threads to scatter
		for (int i=0; i<MaxThreadIndex.X; i++) {
			for (int j=0; j<MaxThreadIndex.Y; j++) {
				for (int k=0; k<MaxThreadIndex.Z; k++) {
					WaitForSingleObject(hMutexMsg[i][j][k],INFINITE);
					Msg[i][j][k] = SCATTER;
					ReleaseMutex(hMutexMsg[i][j][k]);
					ReleaseMutex(hMutexA[i][j][k]);
				}
			}
		}

		// Wait for the workers to finish scattering
		WaitForMultipleObjects(nThreads, hMutexArrayB, true, INFINITE);

		// Tell the worker threads to connect
		for (int i=0; i<MaxThreadIndex.X; i++) {
			for (int j=0; j<MaxThreadIndex.Y; j++) {
				for (int k=0; k<MaxThreadIndex.Z; k++) {
					WaitForSingleObject(hMutexMsg[i][j][k],INFINITE);
					Msg[i][j][k] = CONNECT;
					ReleaseMutex(hMutexMsg[i][j][k]);
					ReleaseMutex(hMutexC[i][j][k]);
				}
			}
		}

		WaitForMultipleObjects(nThreads, hMutexArrayA, true, INFINITE);

		// PROCESSING HERE

		// Increment the number of iterations completed
		nIterations++;

		for (n=0; n<nThreads; n++) {
			ReleaseMutex(hMutexArrayB[n]);
		}

		// Check for empty active sets
		Empty = true;
		for (int i=0; i<MaxThreadIndex.X; i++) {
			for (int j=0; j<MaxThreadIndex.Y; j++) {
				for (int k=0; k<MaxThreadIndex.Z; k++) {
					if (ActiveSet[i][j][k] != NULL) {
						Empty = false;
					}
				}
			}
		}

		printf("Completed %d iterations\n", nIterations);
		for (int i=0; i<MaxThreadIndex.X; i++) {
			for (int j=0; j<MaxThreadIndex.Y; j++) {
				for (int k=0; k<MaxThreadIndex.Z; k++) {
					printf("\tSection (%d,%d,%d):\t%d active junctions\n", i+1, j+1, k+1, ActiveJunctions[i][j][k]);
				}
			}
		}
	}

	// Tell the worker threads to finish
	for (int i=0; i<MaxThreadIndex.X; i++) {
		for (int j=0; j<MaxThreadIndex.Y; j++) {
			for (int k=0; k<MaxThreadIndex.Z; k++) {
				Msg[i][j][k] = END;
				ReleaseMutex(hMutexA[i][j][k]);
			}
		}
	}

	// Wait for the worker threads to terminate
	WaitForMultipleObjects(nThreads, hWorkerThreadArray, true, INFINITE);

	// Free memory allocated to the synchronisation
	FreeResources();

	printf("Algorithm complete, took %d iterations\n", nIterations);
}


// Secondary Thread Function
DWORD WINAPI WorkerThread(LPVOID *lpParam)
{
	ThreadData_t *Data = (ThreadData_t*)lpParam;
	int x = Data->Index.X;
	int y = Data->Index.Y;
	int z = Data->Index.Z;
	Msg_t MsgBuffer;

	hMutexB[x][y][z] = CreateMutex(NULL, FALSE, NULL);
	WaitForSingleObject(hMutexB[x][y][z], INFINITE);

	WaitForSingleObject(hMutexMsg[x][y][z], INFINITE);
	Msg[x][y][z] = READY;
	ReleaseMutex(hMutexMsg[x][y][z]);

	while (1) {
		// Wait until a message has been posted
		WaitForSingleObject(hMutexA[x][y][z], INFINITE);

		// PROCESSING HERE

		// Retrieve the message
		WaitForSingleObject(hMutexMsg[x][y][z], INFINITE);
		MsgBuffer = Msg[x][y][z];
		Msg[x][y][z] = NO_MSG;
		ReleaseMutex(hMutexMsg[x][y][z]);

		switch (MsgBuffer) {
			case NO_MSG:
				//printf("Thread %d received error\n",x);
				//Sleep(2000);
				//exit(1);
				break;
			case SCATTER:
				Scatter(Data);
				break;
			case CONNECT:
				CopyNodeAdditions(Data);
				Connect(Data);
				break;
			case END:
				ExitThread(0);
		}

		ReleaseMutex(hMutexB[x][y][z]);
		WaitForSingleObject(hMutexC[x][y][z], INFINITE);

		// Retrieve the message
		WaitForSingleObject(hMutexMsg[x][y][z], INFINITE);
		MsgBuffer = Msg[x][y][z];
		Msg[x][y][z] = NO_MSG;
		ReleaseMutex(hMutexMsg[x][y][z]);

		switch (MsgBuffer) {
			case NO_MSG:
				//printf("Thread %d received error\n",x);
				//Sleep(2000);
				//exit(1);
				break;
			case SCATTER:
				Scatter(Data);
				break;
			case CONNECT:
				CopyNodeAdditions(Data);
				Connect(Data);
				break;
			case END:
				ExitThread(0);
		}

		ReleaseMutex(hMutexA[x][y][z]);
		WaitForSingleObject(hMutexB[x][y][z], INFINITE);

		// Retrieve the message
		WaitForSingleObject(hMutexMsg[x][y][z], INFINITE);
		MsgBuffer = Msg[x][y][z];
		Msg[x][y][z] = NO_MSG;
		ReleaseMutex(hMutexMsg[x][y][z]);

		switch (MsgBuffer) {
			case NO_MSG:
				//printf("Thread %d received error\n",x);
				//Sleep(2000);
				//exit(1);
				break;
			case SCATTER:
				Scatter(Data);
				break;
			case CONNECT:
				CopyNodeAdditions(Data);
				Connect(Data);
				break;
			case END:
				ExitThread(0);
		}

		ReleaseMutex(hMutexC[x][y][z]);
	}

}


// Single iteration of the TLM algorithm scatter sequence
void Scatter(ThreadData_t *Data) 
{
	double Value;			// Temporary node value
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *NewNode;
	int x, y, z;
	int xIndex = Data->Index.X;
	int yIndex = Data->Index.Y;
	int zIndex = Data->Index.Z;
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
void Connect(ThreadData_t *Data) 
{
	double Value;			// Temporary node value
	double AvgEnergy;		// Average energy over two iterations
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *PreviousNode;
	int xIndex = Data->Index.X;
	int yIndex = Data->Index.Y;
	int zIndex = Data->Index.Z;

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
void CopyNodeAdditions(ThreadData_t *Data)
{
	ActiveNode *TempNode, *CurrentNode, *PreviousNode;
	int xIndex = Data->Index.X;
	int yIndex = Data->Index.Y;
	int zIndex = Data->Index.Z;

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


// Remove nodes no longer in set 1
void CorrectActiveSet(ThreadData_t *Data) 
{
	ActiveNode *CurrentNode, *PreviousNode;
	int xNew,yNew,zNew;
	int xIndex = Data->Index.X;
	int yIndex = Data->Index.Y;
	int zIndex = Data->Index.Z;
	int xMin = Data->xMin;
	int xMax = Data->xMax;
	int yMin = Data->yMin;
	int yMax = Data->yMax;
	int zMin = Data->zMin;
	int zMax = Data->zMax;

	PreviousNode = NULL;
	CurrentNode = ActiveSet[xIndex][yIndex][zIndex];

	while (CurrentNode != NULL) {
		if (CurrentNode->X < xMin || CurrentNode->X > xMax || CurrentNode->Y < yMin || CurrentNode->Y > yMax ||CurrentNode->Z < zMin || CurrentNode->Z > zMax) {
			// Outside of boundaries, calculate which section to place in
			for (int i=0; i<MaxThreadIndex.X; i++) {
				for (int j=0; j<MaxThreadIndex.Y; j++) {
					for (int k=0; k<MaxThreadIndex.Z; k++) {
						if (CurrentNode->X >= ThreadData[i][j][k].xMin || CurrentNode->X <= ThreadData[i][j][k].xMax ||
							CurrentNode->Y >= ThreadData[i][j][k].yMin || CurrentNode->Y <= ThreadData[i][j][k].yMax ||
							CurrentNode->Z >= ThreadData[i][j][k].zMin || CurrentNode->Z <= ThreadData[i][j][k].zMax)
						{
							xNew = i;
							yNew = j;
							zNew = k;
						}
					}
				}
			}
			// In the middle of the list
			if (PreviousNode != NULL) {
				PreviousNode->NextActiveNode = CurrentNode->NextActiveNode;
				CurrentNode->NextActiveNode = ActiveSet[xNew][yNew][zNew];
				ActiveSet[xNew][yNew][zNew] = CurrentNode;
				CurrentNode = PreviousNode->NextActiveNode;
			}
			else {
				// At the start of the list
				ActiveSet[xIndex][yIndex][zIndex] = CurrentNode->NextActiveNode;
				CurrentNode->NextActiveNode = ActiveSet[xNew][yNew][zNew];
				ActiveSet[xNew][yNew][zNew] = CurrentNode;
				CurrentNode = ActiveSet[xIndex][yIndex][zIndex];
			}
			ActiveJunctions[xNew][yNew][zNew]++;
			ActiveJunctions[xIndex][yIndex][zIndex]--;
		}
		else {
			PreviousNode = CurrentNode;
			CurrentNode = CurrentNode->NextActiveNode;
		}
	}
}


void CalculateSectionIndices(void)
{
	int nSections = 1;
	int MaxSizes[3] = {1,1,1};
	int MaxSections = 1;

	for (int i=1; i<=Threads; i++) {
		for (int j=1; j<=Threads/i; j++) {
			for (int k=1; k<= Threads/i/j; k++) {
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
			MaxThreadIndex.X = MaxSizes[0];
			if (ySize >= zSize) {
				MaxThreadIndex.Y = MaxSizes[1];
				MaxThreadIndex.Z = MaxSizes[2];
			}
			else {
				MaxThreadIndex.Z = MaxSizes[1];
				MaxThreadIndex.Y = MaxSizes[2];
			}
		}
		else {
			MaxThreadIndex.Z = MaxSizes[0];
			MaxThreadIndex.X = MaxSizes[1];
			MaxThreadIndex.Y = MaxSizes[2];							
		}
	}
	else {
		if (xSize < zSize) {
			MaxThreadIndex.X = MaxSizes[2];
			if (ySize >= zSize) {
				MaxThreadIndex.Y = MaxSizes[0];
				MaxThreadIndex.Z = MaxSizes[1];
			}
			else {
				MaxThreadIndex.Z = MaxSizes[0];
				MaxThreadIndex.Y = MaxSizes[1];
			}
		}
		else {
			MaxThreadIndex.Y = MaxSizes[0];
			MaxThreadIndex.X = MaxSizes[1];
			MaxThreadIndex.Z = MaxSizes[2];
		}
	}
}


// Allocate multithreading resources
void AllocateResources(void)
{
	ThreadData_t *pData;

	hWorkerThreads = (HANDLE***)malloc(MaxThreadIndex.X*sizeof(HANDLE**));
	dwWorkerThreadIDs = (DWORD***)malloc(MaxThreadIndex.X*sizeof(DWORD**));
	ActiveJunctions = (int***)malloc(MaxThreadIndex.X*sizeof(int**));
	ActiveSet = (ActiveNode****)malloc(MaxThreadIndex.X*sizeof(ActiveNode***));
	NodeAdditions = (ActiveNode*****)malloc(MaxThreadIndex.X*sizeof(ActiveNode****));
	hMutexA = (HANDLE***)malloc(MaxThreadIndex.X*sizeof(HANDLE**));
	hMutexB = (HANDLE***)malloc(MaxThreadIndex.X*sizeof(HANDLE**));
	hMutexC = (HANDLE***)malloc(MaxThreadIndex.X*sizeof(HANDLE**));
	hMutexMsg = (HANDLE***)malloc(MaxThreadIndex.X*sizeof(HANDLE**));
	Msg = (Msg_t***)malloc(MaxThreadIndex.X*sizeof(Msg_t**));
	ThreadData = (ThreadData_t***)malloc(MaxThreadIndex.X*sizeof(ThreadData_t**));

	for (int i=0; i<MaxThreadIndex.X; i++) {
		hWorkerThreads[i] = (HANDLE**)malloc(MaxThreadIndex.Y*sizeof(HANDLE*));
		dwWorkerThreadIDs[i] = (DWORD**)malloc(MaxThreadIndex.Y*sizeof(DWORD*));
		ThreadData[i] = (ThreadData_t**)malloc(MaxThreadIndex.Y*sizeof(ThreadData_t*));
		ActiveJunctions[i] = (int**)malloc(MaxThreadIndex.Y*sizeof(int*));
		ActiveSet[i] = (ActiveNode***)malloc(MaxThreadIndex.Y*sizeof(ActiveNode**));
		NodeAdditions[i] = (ActiveNode****)malloc(MaxThreadIndex.Y*sizeof(ActiveNode***));
		hMutexA[i] = (HANDLE**)malloc(MaxThreadIndex.Y*sizeof(HANDLE*));
		hMutexB[i] = (HANDLE**)malloc(MaxThreadIndex.Y*sizeof(HANDLE*));
		hMutexC[i] = (HANDLE**)malloc(MaxThreadIndex.Y*sizeof(HANDLE*));
		hMutexMsg[i] = (HANDLE**)malloc(MaxThreadIndex.Y*sizeof(HANDLE*));
		Msg[i] = (Msg_t**)malloc(MaxThreadIndex.Y*sizeof(Msg_t*));
		ThreadData[i] = (ThreadData_t**)malloc(MaxThreadIndex.Y*sizeof(ThreadData_t*));

		for (int j=0; j<MaxThreadIndex.Y; j++) {
			hWorkerThreads[i][j] = (HANDLE*)malloc(MaxThreadIndex.Z*sizeof(HANDLE));
			dwWorkerThreadIDs[i][j] = (DWORD*)malloc(MaxThreadIndex.Z*sizeof(DWORD));		
			ThreadData[i][j] = (ThreadData_t*)malloc(MaxThreadIndex.Z*sizeof(ThreadData_t));
			ActiveJunctions[i][j] = (int*)malloc(MaxThreadIndex.Z*sizeof(int));
			ActiveSet[i][j] = (ActiveNode**)malloc(MaxThreadIndex.Z*sizeof(ActiveNode*));
			NodeAdditions[i][j] = (ActiveNode***)malloc(MaxThreadIndex.Z*sizeof(ActiveNode**));
			hMutexA[i][j] = (HANDLE*)malloc(MaxThreadIndex.Z*sizeof(HANDLE));
			hMutexB[i][j] = (HANDLE*)malloc(MaxThreadIndex.Z*sizeof(HANDLE));
			hMutexC[i][j] = (HANDLE*)malloc(MaxThreadIndex.Z*sizeof(HANDLE));
			hMutexMsg[i][j] = (HANDLE*)malloc(MaxThreadIndex.Z*sizeof(HANDLE));
			Msg[i][j] = (Msg_t*)malloc(MaxThreadIndex.Z*sizeof(Msg_t));
			ThreadData[i][j] = (ThreadData_t*)malloc(MaxThreadIndex.Z*sizeof(ThreadData_t));

			for (int k=0; k<MaxThreadIndex.Z; k++) {
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

				// Create the mutexs
				hMutexA[i][j][k] = CreateMutex(NULL, TRUE, NULL);
				hMutexC[i][j][k] = CreateMutex(NULL, FALSE, NULL);
				hMutexMsg[i][j][k] = CreateMutex(NULL, FALSE, NULL);

				// Initialise the messages
				Msg[i][j][k] = NO_MSG;

				// Initialise the thread index
				pData = &ThreadData[i][j][k];
				pData->Index.X = i;
				pData->Index.Y = j;
				pData->Index.Z = k;

				hWorkerThreads[i][j][k] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, (LPVOID)&ThreadData[i][j][k], 0, &dwWorkerThreadIDs[i][j][k]);
				if (hWorkerThreads[i] == NULL) {
					printf("Worker thread for block (%d,%d,%d) could not be started\n", i+1, j+1, k+1);
					exit(1);
				}
				else {
					printf("Worker thread (%d,%d,%d) started\n", i+1,j+1,k+1);
				}
		

			}
		}
	}
}


// Removed allocated memory for thread control
void FreeResources(void)
{
	for (int i=0; i<MaxThreadIndex.X; i++) {
		for (int j=0; j<MaxThreadIndex.Y; j++) {
			free(hWorkerThreads[i][j]);
			free(dwWorkerThreadIDs[i][j]);
			free(ActiveJunctions[i][j]);
			free(ActiveSet[i][j]);
			free(NodeAdditions[i][j]);
			free(hMutexA[i][j]);
			free(hMutexB[i][j]);
			free(hMutexC[i][j]);
			free(hMutexMsg[i][j]);
			free(Msg[i][j]);
			free(ThreadData[i][j]);
		}
		free(hWorkerThreads[i]);
		free(dwWorkerThreadIDs[i]);
		free(ActiveJunctions[i]);
		free(ActiveSet[i]);
		free(NodeAdditions[i]);
		free(hMutexA[i]);
		free(hMutexB[i]);
		free(hMutexC[i]);
		free(hMutexMsg[i]);
		free(Msg[i]);
		free(ThreadData[i]);
	}
	free(hWorkerThreads);
	free(dwWorkerThreadIDs);
	free(ActiveJunctions);
	free(ActiveSet);
	free(NodeAdditions);
	free(hMutexA);
	free(hMutexB);
	free(hMutexC);
	free(hMutexMsg);
	free(Msg);
	free(ThreadData);
}


void CalculateInitialBoundaries(void)
{
	for (int i=0; i<MaxThreadIndex.X; i++) {
		for (int j=0; j<MaxThreadIndex.Y; j++) {
			for (int k=0; k<MaxThreadIndex.Z; k++) {
				ThreadData[i][j][k].xMin = RoundToNearest(i*xSize/MaxThreadIndex.X);
				ThreadData[i][j][k].xMax = RoundToNearest((i+1)*xSize/MaxThreadIndex.X-1);
				ThreadData[i][j][k].yMin = RoundToNearest(j*ySize/MaxThreadIndex.Y);
				ThreadData[i][j][k].yMax = RoundToNearest((j+1)*ySize/MaxThreadIndex.Y-1);
				ThreadData[i][j][k].zMin = RoundToNearest(k*zSize/MaxThreadIndex.Z);
				ThreadData[i][j][k].zMax = RoundToNearest((k+1)*zSize/MaxThreadIndex.Z-1);
				if (ImpulseSource.X >= ThreadData[i][j][k].xMin && ImpulseSource.X <= ThreadData[i][j][k].xMax &&
					ImpulseSource.Y >= ThreadData[i][j][k].yMin && ImpulseSource.Y <= ThreadData[i][j][k].yMax &&
					ImpulseSource.Z >= ThreadData[i][j][k].zMin && ImpulseSource.Z <= ThreadData[i][j][k].zMax) 
				{
					ActiveJunctions[i][j][k] = 1;
					ActiveSet[i][j][k] = AddJunctionToSet(ImpulseSource.X, ImpulseSource.Y, ImpulseSource.Z, true);
				}
			}
		}
	}
}