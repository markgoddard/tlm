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


// Definitions
#define SCATTER_EVENT	WAIT_OBJECT_0
#define CONNECT_EVENT	WAIT_OBJECT_0+1
#define END_EVENT		WAIT_OBJECT_0+2


// external variables
extern Node ***Grid;
extern int xSize, ySize, zSize;
extern Source ImpulseSource;
extern InputFlags InputData;

extern double Frequency;
extern double MaxPathLoss;
extern double RelativeThreshold;
extern double GridSpacing;
extern int Threads;


// Global variables
double AbsoluteThreshold;

static int *ActiveJunctions;
static ActiveNode **ActiveSet;
static ActiveNode **NodeAdditions;
static HANDLE *hReadyEvent;
static HANDLE *hScatterEvent;
static HANDLE *hHalfScatterEvent;
static HANDLE *hConnectEvent;
static HANDLE *hEndEvent;
static HANDLE *hWorkerThreads;
static DWORD *dwWorkerThreadIDs;
static int Sets;


// Function prototypes
DWORD WINAPI WorkerThread(LPVOID *lpParam);
void Scatter(int SetNumber);
void Connect(int SetNumber);
void EvaluateSource(int Iteration);
ActiveNode *AddJunctionToSet(int x, int y, int z);
ActiveNode *RemoveJunctionFromSet(int x, int y, int z, ActiveNode *InactiveNode);
void CopyNodeAdditions(int SetNumber);
void SetupNodeAdditions(void);
void AllocateResources(void);
void FreeResources(void);


// Top level loop for TLM algorithm
void MainLoop(void)
{
	int nIterations = 0;
	bool Empty = false;

	// Calculate the absolute threshold from the path loss
	AbsoluteThreshold = SQUARE(4*M_PI*GridSpacing/KAPPA*Frequency/SPEED_OF_LIGHT)*pow(10, MaxPathLoss/10.0);
	RelativeThreshold *= RelativeThreshold;
	Sets = 2*Threads;

	// Calculate the boundaries
	AllocateResources();

	// Evaluate source output
	if (nIterations < ImpulseSource.Duration) {
		EvaluateSource(nIterations);
	}
	ActiveJunctions[(ImpulseSource.X+ImpulseSource.Y+ImpulseSource.Z)%Threads] = 1;
	ActiveSet[(ImpulseSource.X+ImpulseSource.Y+ImpulseSource.Z)%Threads] = AddJunctionToSet(ImpulseSource.X, ImpulseSource.Y, ImpulseSource.Z);

	// Wait for the workers to become ready
	WaitForMultipleObjects(Threads, hReadyEvent, true, INFINITE);

	// Repeat the algorithm while the active set is not empty
	while (Empty == false) {

		SetupNodeAdditions();
		
		// Tell the worker threads to scatter
		for (int i=0; i<Threads; i++) {
			SetEvent(hScatterEvent[i]);
		}

		// Wait for the workers to finish scattering
		WaitForMultipleObjects(Threads, hReadyEvent, true, INFINITE);

		// Tell the worker threads to connect
		for (int i=0; i<Threads; i++) {
			SetEvent(hConnectEvent[i]);
		}

		WaitForMultipleObjects(Threads, hReadyEvent, true, INFINITE);

		// Increment the number of iterations completed
		nIterations++;

		// Check for empty active sets
		Empty = true;
		for (int i=0; i<Sets; i++) {
			if (ActiveSet[i] != NULL) {
				Empty = false;
			}
		}
	
		printf("Completed %d iterations\n", nIterations);
		for (int i=0; i<Sets; i++) {
			printf("\tSet %d:\t%d active junctions\n", i+1, ActiveJunctions[i]);
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
	int ThreadNumber = (int)(long long int)lpParam;
	int Set1 = 2*ThreadNumber-(ThreadNumber&0x01);
	int Set2 = (Set1+2)%Sets;
	HANDLE hEventArray[3];
	DWORD EventBuffer;
	
	hEventArray[0] = hScatterEvent[ThreadNumber];
	hEventArray[1] = hConnectEvent[ThreadNumber];
	hEventArray[2] = hEndEvent[ThreadNumber];


	SetEvent(hReadyEvent[ThreadNumber]);

	while (1) {
		// Wait until a message has been posted
		EventBuffer = WaitForMultipleObjects(3, hEventArray, false, INFINITE);

		// Retrieve the message

		switch (EventBuffer) {
			case SCATTER_EVENT:
				Scatter(Set1);
				SetEvent(hHalfScatterEvent[ThreadNumber]);
				WaitForSingleObject(hHalfScatterEvent[(ThreadNumber+2)%Threads], INFINITE);
				Scatter(Set2);
				break;

			case CONNECT_EVENT:
				CopyNodeAdditions(Set1);
				CopyNodeAdditions(Set2);
				Connect(Set1);
				Connect(Set2);
				break;

			case END_EVENT:
				ExitThread(0);
				break;

			default:
				printf("Recieved unknown event code for thread %d\n",ThreadNumber);
				Sleep(1000);
				exit(1);
		}

		SetEvent(hReadyEvent[ThreadNumber]);
	}
}


// Single iteration of the TLM algorithm scatter sequence
void Scatter(int SetNumber) 
{
	double Value;			// Temporary node value
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *NewNode;
	int NextSet = (SetNumber+1)%Sets;
	int PreviousSet = (SetNumber+Sets-1)%Sets;
	int x,y,z;

	// Setup the current node pointer
	CurrentNode = ActiveSet[SetNumber];

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
				NewNode->NextActiveNode = NodeAdditions[NextSet];
				NodeAdditions[NextSet] = NewNode;
				ActiveJunctions[NextSet]++;
			}
		}

		// Positive y direction
		if (y < (ySize - 1)) {
			if (Grid[x][y+1][z].Active == false && Grid[x][y+1][z].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x,y+1,z);
				NewNode->NextActiveNode = NodeAdditions[NextSet];
				NodeAdditions[NextSet] = NewNode;
				ActiveJunctions[NextSet]++;
			}				
		}

		// Positive z direction
		if (z < (zSize - 1)) {
			if (Grid[x][y][z+1].Active == false && Grid[x][y][z+1].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x,y,z+1);
				NewNode->NextActiveNode = NodeAdditions[NextSet];
				NodeAdditions[NextSet] = NewNode;
				ActiveJunctions[NextSet]++;
			}
		}

		// Negative x direction
		if (x > 0) {
			if (Grid[x-1][y][z].Active == false && Grid[x-1][y][z].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x-1,y,z);
				NewNode->NextActiveNode = NodeAdditions[PreviousSet];
				NodeAdditions[PreviousSet] = NewNode;
				ActiveJunctions[PreviousSet]++;
			}
		}

		// Negative y direction
		if (y > 0) {
			if (Grid[x][y-1][z].Active == false && Grid[x][y-1][z].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x,y-1,z);
				NewNode->NextActiveNode = NodeAdditions[PreviousSet];
				NodeAdditions[PreviousSet] = NewNode;
				ActiveJunctions[PreviousSet]++;
			}
		}

		// Negative y direction
		if (z > 0) {
			if (Grid[x][y][z-1].Active == false && Grid[x][y][z-1].PropagateFlag == true) {
				NewNode = AddJunctionToSet(x,y,z-1);
				NewNode->NextActiveNode = NodeAdditions[PreviousSet];
				NodeAdditions[PreviousSet] = NewNode;
				ActiveJunctions[PreviousSet]++;
			}
		}

		// Get the next node in the active set
		CurrentNode = CurrentNode->NextActiveNode;
	}
}



// Single iteration of the TLM algorithm connent sequence
void Connect(int SetNumber) 
{
	double Value;			// Temporary node value
	double AvgEnergy;		// Average energy over two iterations
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *PreviousNode;

	int x, y, z;

	// Connect phase, compute the junction inputs for all of the junctions in the active set
	PreviousNode = NULL;
	CurrentNode = ActiveSet[SetNumber];

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
			ActiveJunctions[SetNumber]--;
			if (PreviousNode != NULL) {
				PreviousNode->NextActiveNode = CurrentNode;
			}
			else {
				ActiveSet[SetNumber] = CurrentNode;
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
void CopyNodeAdditions(int SetNumber)
{
	// Add the list to the head of the main list
	ActiveSet[SetNumber] = NodeAdditions[SetNumber];
}

void SetupNodeAdditions(void)
{
	for (int i=0; i<Sets; i++) {
		NodeAdditions[i] = ActiveSet[i];
	}
}


// Allocate multithreading resources
void AllocateResources(void)
{
	hWorkerThreads = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	dwWorkerThreadIDs = (DWORD*)malloc(Threads*sizeof(DWORD));
	ActiveJunctions = (int*)malloc(Sets*sizeof(int));
	ActiveSet = (ActiveNode**)malloc(Sets*sizeof(ActiveNode*));
	NodeAdditions = (ActiveNode**)malloc(Sets*sizeof(ActiveNode*));
	hReadyEvent = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	hScatterEvent = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	hHalfScatterEvent = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	hConnectEvent = (HANDLE*)malloc(Threads*sizeof(HANDLE));
	hEndEvent = (HANDLE*)malloc(Threads*sizeof(HANDLE));

	for (int i=0; i<Threads; i++) {

		// Create the events
		hReadyEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		hScatterEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		hHalfScatterEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		hConnectEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		hEndEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);

		hWorkerThreads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, (LPVOID)(long long int)i, 0, &dwWorkerThreadIDs[i]);
		if (hWorkerThreads[i] == NULL) {
			printf("Worker thread %d could not be started\n", i+1);
			exit(1);
		}
		else {
			printf("Worker thread %d started\n", i+1);
		}
	}

	for (int i=0; i<Sets; i++) {
		// Initialise the active junctions
		ActiveJunctions[i] = 0;
	
		// Initialise the list heads
		ActiveSet[i] = NULL;
		NodeAdditions[i] = NULL;
	}
}


// Removed allocated memory for thread control
void FreeResources(void)
{
	free(hWorkerThreads);
	free(dwWorkerThreadIDs);
	free(ActiveJunctions);
	free(ActiveSet);
	free(NodeAdditions);
	free(hReadyEvent);
	free(hScatterEvent);
	free(hHalfScatterEvent);
	free(hConnectEvent);
	free(hEndEvent);
}
