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
						DO_NOTHING,
						READY,
						SCATTER_POSITIVE,
						SCATTER_NEGATIVE,
						CONNECT,
						END
						} Msg_t;


// Global variables
static int *ActiveJunctions;
double AbsoluteThreshold;

static ActiveNode **ActiveSet;
static ActiveNode **NodeAdditions;
static HANDLE **hMutex;
static HANDLE *hMutexMsg;
static Msg_t *Msg;
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


// Function prototypes
DWORD WINAPI WorkerThread(LPVOID *lpParam);
void ScatterPositive(int ThreadNumber);
void ScatterNegative(int ThreadNumber);
void Connect(int ThreadNumber);
void EvaluateSource(int Iteration);
ActiveNode *AddJunctionToSet(int x, int y, int z);
ActiveNode *RemoveJunctionFromSet(int x, int y, int z, ActiveNode *InactiveNode);
void CopyNodeAdditions(int ThreadNumber);
void SetupNodeAdditions(void);
void AllocateResources(void);
void FreeResources(void);


// Top level loop for TLM algorithm
void MainLoop(void)
{
	int nIterations = 0;
	int Index = 0;
	Msg_t *MsgBuffer;
	bool Success;
	bool Empty = false;

	// Calculate the absolute threshold from the path loss
	AbsoluteThreshold = SQUARE(4*M_PI*GridSpacing/KAPPA*Frequency/SPEED_OF_LIGHT)*pow(10, MaxPathLoss/10.0);
	RelativeThreshold *= RelativeThreshold;

	// Calculate the boundaries
	AllocateResources();

	// Allocate memory for the message buffer and initialise to no message
	MsgBuffer = (Msg_t*)malloc(Threads*sizeof(Msg_t));

	// Evaluate source output
	if (nIterations < ImpulseSource.Duration) {
		EvaluateSource(nIterations);
	}
	ActiveJunctions[(ImpulseSource.X+ImpulseSource.Y+ImpulseSource.Z)%Threads] = 1;
	ActiveSet[(ImpulseSource.X+ImpulseSource.Y+ImpulseSource.Z)%Threads] = AddJunctionToSet(ImpulseSource.X, ImpulseSource.Y, ImpulseSource.Z);

	// Wait for the workers to become ready
	do {
		Sleep(1);
		Success = true;
		for (int i=0; i<Threads; i++) {
			WaitForSingleObject(hMutexMsg[i], INFINITE);
			if (Msg[i] != READY) {
				Success = false;
			}
			ReleaseMutex(hMutexMsg[i]);
		}
	}
	while (Success == false);

	// Repeat the algorithm while the active set is not empty
	while (Empty == false) {

		SetupNodeAdditions();
		
		// Tell the worker threads to scatter
		for (int i=0; i<Threads; i++) {
			Msg[i] = SCATTER_POSITIVE;
			ReleaseMutex(hMutex[Index][i]);
		}
		++Index %= 3;

		// Wait for the workers to finish scattering
		WaitForMultipleObjects(Threads, hMutex[Index], true, INFINITE);
		++Index %= 3;

		// Tell the worker threads to scatter
		for (int i=0; i<Threads; i++) {
			Msg[i] = SCATTER_NEGATIVE;
			ReleaseMutex(hMutex[Index][i]);
		}
		++Index %= 3;

		// Wait for the workers to finish scattering
		WaitForMultipleObjects(Threads, hMutex[Index], true, INFINITE);
		++Index %= 3;

		// Tell the worker threads to connect
		for (int i=0; i<Threads; i++) {
			Msg[i] = CONNECT;
			ReleaseMutex(hMutex[Index][i]);
		}
		++Index %= 3;

		WaitForMultipleObjects(Threads, hMutex[Index], true, INFINITE);
		++Index %= 3;

		// PROCESSING HERE

		// Increment the number of iterations completed
		nIterations++;

		// Check for empty active sets
		Empty = true;
		for (int i=0; i<Threads; i++) {
			if (ActiveSet[i] != NULL) {
				Empty = false;
			}
		}
	
		printf("Completed %d iterations\n", nIterations);
		for (int i=0; i<Threads; i++) {
			printf("\tThread %d:\t%d active junctions\n", i+1, ActiveJunctions[i]);
		}
	}

	// Tell the worker threads to finish
	for (int i=0; i<Threads; i++) {
		Msg[i] = END;
		ReleaseMutex(hMutex[Index][i]);
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
	int ThreadNumber = (int)(long long int)lpParam;
	int Index = 0;

	Msg_t MsgBuffer;

	hMutex[1][ThreadNumber] = CreateMutex(NULL, TRUE, NULL);

	WaitForSingleObject(hMutexMsg[ThreadNumber], INFINITE);
	Msg[ThreadNumber] = READY;
	ReleaseMutex(hMutexMsg[ThreadNumber]);

	while (1) {
		// Wait until a message has been posted
		WaitForSingleObject(hMutex[Index][ThreadNumber], INFINITE);
		++Index %= 3;

		// Retrieve the message
		MsgBuffer = Msg[ThreadNumber];
		Msg[ThreadNumber] = NO_MSG;

		switch (MsgBuffer) {
			case NO_MSG:
				printf("Thread %d received error\n", ThreadNumber+1);
				Sleep(2000);
				exit(1);
				break;
			case SCATTER_POSITIVE:
				ScatterPositive(ThreadNumber);
				break;
			case SCATTER_NEGATIVE:
				ScatterNegative(ThreadNumber);
				break;
			case CONNECT:
				CopyNodeAdditions(ThreadNumber);
				Connect(ThreadNumber);
				break;
			case DO_NOTHING:
				break;
			case END:
				ExitThread(0);
		}

		ReleaseMutex(hMutex[Index][ThreadNumber]);
		
		++Index %= 3;
	}
}


// Single iteration of the TLM algorithm scatter sequence
void ScatterPositive(int ThreadNumber) 
{
	double Value;			// Temporary node value
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *NewNode;
	int NextThread = (ThreadNumber+1)%Threads;
	int x,y,z;

	// Setup the current node pointer
	CurrentNode = ActiveSet[ThreadNumber];

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
			if (Grid[x+1][y][z].Active == false && Grid[x+1][y][z].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x+1,y,z);
				NewNode->NextActiveNode = NodeAdditions[NextThread];
				NodeAdditions[NextThread] = NewNode;
				ActiveJunctions[NextThread]++;
			}
		}

		// Positive y direction
		if (y < (ySize - 1)) {
			if (Grid[x][y+1][z].Active == false && Grid[x][y+1][z].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x,y+1,z);
				NewNode->NextActiveNode = NodeAdditions[NextThread];
				NodeAdditions[NextThread] = NewNode;
				ActiveJunctions[NextThread]++;
			}				
		}

		// Positive z direction
		if (z < (zSize - 1)) {
			if (Grid[x][y][z+1].Active == false && Grid[x][y][z+1].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x,y,z+1);
				NewNode->NextActiveNode = NodeAdditions[NextThread];
				NodeAdditions[NextThread] = NewNode;
				ActiveJunctions[NextThread]++;
			}
		}

		// Get the next node in the active set
		CurrentNode = CurrentNode->NextActiveNode;
	}
}


// Single iteration of the TLM algorithm scatter sequence
void ScatterNegative(int ThreadNumber) 
{
	ActiveNode *CurrentNode;
	ActiveNode *NewNode;
	int PreviousThread = (ThreadNumber+Threads-1)%Threads;
	int x,y,z;

	// Setup the current node pointer
	CurrentNode = ActiveSet[ThreadNumber];

	// Scatter phase, compute the junction outputs for all of the junctions in the active set
	while (CurrentNode != NULL) {
		x = CurrentNode->X;
		y = CurrentNode->Y;
		z = CurrentNode->Z;

		// Check whether adjacent nodes need to be added to the active junction set

		// Negative x direction
		if (x > 0) {
			if (Grid[x-1][y][z].Active == false && Grid[x-1][y][z].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x-1,y,z);
				NewNode->NextActiveNode = NodeAdditions[PreviousThread];
				NodeAdditions[PreviousThread] = NewNode;
				ActiveJunctions[PreviousThread]++;
			}
		}

		// Negative y direction
		if (y > 0) {
			if (Grid[x][y-1][z].Active == false && Grid[x][y-1][z].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x,y-1,z);
				NewNode->NextActiveNode = NodeAdditions[PreviousThread];
				NodeAdditions[PreviousThread] = NewNode;
				ActiveJunctions[PreviousThread]++;
			}
		}

		// Negative y direction
		if (z > 0) {
			if (Grid[x][y][z-1].Active == false && Grid[x][y][z-1].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x,y,z-1);
				NewNode->NextActiveNode = NodeAdditions[PreviousThread];
				NodeAdditions[PreviousThread] = NewNode;
				ActiveJunctions[PreviousThread]++;
			}
		}
		// Get the next node in the active set
		CurrentNode = CurrentNode->NextActiveNode;
	}
}


// Single iteration of the TLM algorithm connent sequence
void Connect(int ThreadNumber) 
{
	double Value;			// Temporary node value
	double AvgEnergy;		// Average energy over two iterations
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *PreviousNode;

	int x, y, z;

	// Connect phase, compute the junction inputs for all of the junctions in the active set
	PreviousNode = NULL;
	CurrentNode = ActiveSet[ThreadNumber];

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
			ActiveJunctions[ThreadNumber]--;
			if (PreviousNode != NULL) {
				PreviousNode->NextActiveNode = CurrentNode;
			}
			else {
				ActiveSet[ThreadNumber] = CurrentNode;
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


// Return a newly allocated ActiveNode structure with the coordinates given
ActiveNode *AddJunctionToSet(int x, int y, int z)
{
	ActiveNode *NewNode;

	NewNode = (ActiveNode*)malloc(sizeof(ActiveNode)); 


	NewNode->X = x;
	NewNode->Y = y;
	NewNode->Z = z;
	NewNode->NextActiveNode = NULL;
	Grid[x][y][z].Active = true;

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
void CopyNodeAdditions(int ThreadNumber)
{
	// Add the list to the head of the main list
	ActiveSet[ThreadNumber] = NodeAdditions[ThreadNumber];
}

void SetupNodeAdditions(void)
{
	for (int i=0; i<Threads; i++) {
		NodeAdditions[i] = ActiveSet[i];
	}
}


// Allocate multithreading resources
void AllocateResources(void)
{
	hWorkerThreads = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	dwWorkerThreadIDs = (DWORD*)malloc(Threads*sizeof(DWORD));
	ActiveJunctions = (int*)malloc(Threads*sizeof(int));
	ActiveSet = (ActiveNode**)malloc(Threads*sizeof(ActiveNode*));
	NodeAdditions = (ActiveNode**)malloc(Threads*sizeof(ActiveNode*));
	hMutex = (HANDLE**)malloc(3*sizeof(HANDLE*));
	hMutexMsg = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	Msg = (Msg_t*)malloc(Threads*sizeof(Msg_t));

	// Allocate memory for each of the three mutex vectors
	for (int i=0; i<3; i++) {
		hMutex[i] = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	}

	for (int i=0; i<Threads; i++) {

		// Initialise the active junctions
		ActiveJunctions[i] = 0;
	
		// Initialise the list heads
		ActiveSet[i] = NULL;
		NodeAdditions[i] = NULL;

		// Create the mutexs
		hMutex[0][i] = CreateMutex(NULL, TRUE, NULL);
		hMutex[2][i] = CreateMutex(NULL, TRUE, NULL);
		hMutexMsg[i] = CreateMutex(NULL, FALSE, NULL);

		// Initialise the messages
		Msg[i] = NO_MSG;

		hWorkerThreads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, (LPVOID)(long long int)i, 0, &dwWorkerThreadIDs[i]);
		if (hWorkerThreads[i] == NULL) {
			printf("Worker thread %d could not be started\n", i+1);
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
	for (int i=0; i<3; i++) {
		free(hMutex[i]);
	}
	free(hWorkerThreads);
	free(dwWorkerThreadIDs);
	free(ActiveJunctions);
	free(ActiveSet);
	free(NodeAdditions);
	free(hMutex);
	free(hMutexMsg);
	free(Msg);
}
