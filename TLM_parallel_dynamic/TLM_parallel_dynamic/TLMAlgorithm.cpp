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


// Type Definitions
typedef volatile enum {
						NO_MSG,
						READY,
						SCATTER,
						CONNECT,
						END
						} Msg_t;


// Global variables
static int ActiveJunctions[2] = {0,0};
double AbsoluteThreshold;

static ActiveNode *ActiveSet[2] = {NULL,NULL};
static ActiveNode *NodeAdditions[2] = {NULL,NULL};
static HANDLE hMutexA[2];
static HANDLE hMutexB[2];
static HANDLE hMutexC[2];
static HANDLE hMutexMsg[2];
static Msg_t Msg[2] = {NO_MSG, NO_MSG};
static int xMin[2];
static int xMax[2];

extern Node ***Grid;
extern int xSize, ySize, zSize;
extern Source ImpulseSource;
extern InputFlags InputData;

extern double Frequency;
extern double MaxPathLoss;
extern double RelativeThreshold;
extern double GridSpacing;


// Function prototypes
DWORD WINAPI WorkerThread(LPVOID *lpParam);
void Scatter(int ThreadNumber, int xMin, int xMax);
void Connect(int ThreadNumber);
void EvaluateSource(int Iteration);
void CalculateBoundary(void);
ActiveNode *AddJunctionToSet(int x, int y, int z, bool Active);
ActiveNode *RemoveJunctionFromSet(int x, int y, int z, ActiveNode *InactiveNode);
void CopyNodeAdditions(ActiveNode **pHead, ActiveNode **pAdditionsHead, int *nActive);
void CorrectActiveSet(int Set1);



// Top level loop for TLM algorithm
void MainLoop(void)
{
	int nIterations = 0;
	HANDLE hWorkerThreads[2];
	DWORD dwWorkerThreadIDs[2];
	Msg_t MsgBuffer[2];
	int ThreadNumber[2] = {0,1};

	// Calculate the absolute threshold from the path loss
	AbsoluteThreshold = SQUARE(4*M_PI*GridSpacing/KAPPA*Frequency/SPEED_OF_LIGHT)*pow(10, MaxPathLoss/10.0);
	RelativeThreshold *= RelativeThreshold;

	// Add the impulse junction to the active set
	ActiveJunctions[0] = 1;
	ActiveSet[0] = AddJunctionToSet(ImpulseSource.X, ImpulseSource.Y, ImpulseSource.Z, true);

	xMin[0] = 0;
	xMax[0] = MAX(ImpulseSource.X,0);
	xMin[1] = xMax[0]+1;
	xMax[1] = xSize-1;

	// Create the mutex objects
	hMutexA[0] = CreateMutex(NULL, TRUE, NULL);
	hMutexA[1] = CreateMutex(NULL, TRUE, NULL);
	hMutexC[0] = CreateMutex(NULL, FALSE, NULL);
	hMutexC[1] = CreateMutex(NULL, FALSE, NULL);
	hMutexMsg[0] = CreateMutex(NULL, FALSE, NULL);
	hMutexMsg[1] = CreateMutex(NULL, FALSE, NULL);

	// Start the secondary thread
	for (int i=0; i<2; i++) {
		hWorkerThreads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WorkerThread, (LPVOID)&ThreadNumber[i], 0, &dwWorkerThreadIDs[i]);
		if (hWorkerThreads[i] == NULL) {
			printf("Worker thread %d could not be started\n", i);
			exit(1);
		}
		else {
			printf("Worker thread %d started\n", i);
		}
	}

	// Evaluate source output
	if (nIterations < ImpulseSource.Duration) {
		EvaluateSource(nIterations);
	}

	// Wait for the workers to become ready
	MsgBuffer[0] = NO_MSG;
	MsgBuffer[1] = NO_MSG;
	do {
		for (int i=0; i<2; i++) {
			if (MsgBuffer[i] != READY) {
				WaitForSingleObject(hMutexMsg[i], INFINITE);
				MsgBuffer[i] = Msg[i];
				ReleaseMutex(hMutexMsg[i]);
			}
		}
	}
	while (MsgBuffer[0] != READY || MsgBuffer[1] != READY);

	// Repeat the algorithm while the active set is not empty
	while (ActiveSet[0] != NULL || ActiveSet[1] != NULL) {

		if (nIterations ==21) {
			nIterations = 21;
		}

		WaitForMultipleObjects(2, hMutexC, true, INFINITE);
		// Tell the worker threads to scatter
		for (int i=0; i<2; i++) {
			WaitForSingleObject(hMutexMsg[i], INFINITE);
			Msg[i] = SCATTER;
			ReleaseMutex(hMutexMsg[i]);
			ReleaseMutex(hMutexA[i]);
		}

		// Wait for the workers to acknowledge
		WaitForMultipleObjects(2, hMutexB, true, INFINITE);

		for (int i=0; i<2; i++) {
			ReleaseMutex(hMutexC[i]);
		}

		WaitForMultipleObjects(2, hMutexA, true, INFINITE);

		// PROCESSING HERE

		ReleaseMutex(hMutexB[0]);
		ReleaseMutex(hMutexB[1]);

		WaitForMultipleObjects(2, hMutexC, true, INFINITE);
		// Tell the worker threads to scatter
		for (int i=0; i<2; i++) {
			WaitForSingleObject(hMutexMsg[i], INFINITE);
			Msg[i] = CONNECT;
			ReleaseMutex(hMutexMsg[i]);
			ReleaseMutex(hMutexA[i]);
		}

		// Wait for the workers to acknowledge
		WaitForMultipleObjects(2, hMutexB, true, INFINITE);

		for (int i=0; i<2; i++) {
			ReleaseMutex(hMutexC[i]);
		}

		WaitForMultipleObjects(2, hMutexA, true, INFINITE);

		// PROCESSING HERE
		if (nIterations%10 == 9) {
			CalculateBoundary();
		}

		ReleaseMutex(hMutexB[0]);
		ReleaseMutex(hMutexB[1]);

		// Increment the number of iterations completed
		nIterations++;

		printf("Completed %d iterations, A:\t%d\tB:\t%d\tTotal:\t%d\n", nIterations, ActiveJunctions[0], ActiveJunctions[1], ActiveJunctions[0]+ActiveJunctions[1]);

	}

	// Tell the worker threads to finish
	for (int i=0; i<2; i++) {
		WaitForSingleObject(hMutexMsg[i], INFINITE);
		Msg[i] = END;
		ReleaseMutex(hMutexMsg[i]);
		ReleaseMutex(hMutexA[i]);
	}

	printf("Algorithm complete, took %d iterations\n", nIterations);
}


// Secondary Thread Function
DWORD WINAPI WorkerThread(LPVOID *lpParam)
{
	int ThreadNumber = (int)*lpParam;
	Msg_t MsgBuffer;

	hMutexB[ThreadNumber] = CreateMutex(NULL, TRUE, NULL);

	WaitForSingleObject(hMutexMsg[ThreadNumber], INFINITE);
	Msg[ThreadNumber] = READY;
	ReleaseMutex(hMutexMsg[ThreadNumber]);

	while (1) {
		// Wait until a message has been posted
		WaitForSingleObject(hMutexA[ThreadNumber], INFINITE);

		// PROCESSING HERE

		// Retrieve the message
		WaitForSingleObject(hMutexMsg[ThreadNumber], INFINITE);
		MsgBuffer = Msg[ThreadNumber];
		Msg[ThreadNumber] = NO_MSG;
		ReleaseMutex(hMutexMsg[ThreadNumber]);

		switch (MsgBuffer) {
			case NO_MSG:
				exit(1);
			case SCATTER:
				Scatter(ThreadNumber, xMin[ThreadNumber], xMax[ThreadNumber]);
				break;
			case CONNECT:
				CopyNodeAdditions(&ActiveSet[ThreadNumber], &NodeAdditions[ThreadNumber], &ActiveJunctions[ThreadNumber]);
				Connect(ThreadNumber);
				break;
			case END:
				ExitThread(0);
		}

		ReleaseMutex(hMutexB[ThreadNumber]);
		WaitForSingleObject(hMutexC[ThreadNumber], INFINITE);
		ReleaseMutex(hMutexA[ThreadNumber]);
		WaitForSingleObject(hMutexB[ThreadNumber], INFINITE);
		ReleaseMutex(hMutexC[ThreadNumber]);
	}

}


// Single iteration of the TLM algorithm scatter sequence
void Scatter(int ThreadNumber, int xMin, int xMax) 
{
	double Value;			// Temporary node value
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	int x, y, z;

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
			if (x == xMax) {
				if (Grid[x+1][y][z].Active == false && Grid[x+1][y][z].PropagateFlag == true) {
					ActiveNode *NewNode;

					NewNode = AddJunctionToSet(x+1,y,z, false);
					NewNode->NextActiveNode = NodeAdditions[1-ThreadNumber];
					NodeAdditions[1-ThreadNumber] = NewNode;
				}
			}
			else {
				if (Grid[x+1][y][z].Active == false && Grid[x+1][y][z].PropagateFlag == true) {
					ActiveNode *NewNode;

					NewNode = AddJunctionToSet(x+1,y,z, true);
					NewNode->NextActiveNode = ActiveSet[ThreadNumber];
					ActiveSet[ThreadNumber] = NewNode;
					ActiveJunctions[ThreadNumber]++;
				}
			}
		
		}
		// Negative x direction
		if (x > 0) {
			if (x == xMin) {
				if (Grid[x-1][y][z].Active == false && Grid[x-1][y][z].PropagateFlag == true) {
					ActiveNode *NewNode;

					NewNode = AddJunctionToSet(x-1,y,z, false);
					NewNode->NextActiveNode = NodeAdditions[1-ThreadNumber];
					NodeAdditions[1-ThreadNumber] = NewNode;
				}
			}
			else {
				if (Grid[x-1][y][z].Active == false && Grid[x-1][y][z].PropagateFlag == true) {
					ActiveNode *NewNode;

					NewNode = AddJunctionToSet(x-1,y,z, true);
					NewNode->NextActiveNode = ActiveSet[ThreadNumber];
					ActiveSet[ThreadNumber] = NewNode;
					ActiveJunctions[ThreadNumber]++;
				}
			}
		}
		// Positive y direction
		if (y < (ySize - 1)) {
			if (Grid[x][y+1][z].Active == false && Grid[x][y+1][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y+1,z, true);
				NewNode->NextActiveNode = ActiveSet[ThreadNumber];
				ActiveSet[ThreadNumber] = NewNode;
				ActiveJunctions[ThreadNumber]++;
			}
		}
		// Negative y direction
		if (y > 0) {
			if (Grid[x][y-1][z].Active == false && Grid[x][y-1][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y-1,z, true);
				NewNode->NextActiveNode = ActiveSet[ThreadNumber];
				ActiveSet[ThreadNumber] = NewNode;
				ActiveJunctions[ThreadNumber]++;
			}
		}
		// Positive z direction
		if (z < (zSize - 1)) {
			if (Grid[x][y][z+1].Active == false && Grid[x][y][z+1].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y,z+1, true);
				NewNode->NextActiveNode = ActiveSet[ThreadNumber];
				ActiveSet[ThreadNumber] = NewNode;
				ActiveJunctions[ThreadNumber]++;
			}
		}
		// Negative y direction
		if (z > 0) {
			if (Grid[x][y][z-1].Active == false && Grid[x][y][z-1].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y,z-1, true);
				NewNode->NextActiveNode = ActiveSet[ThreadNumber];
				ActiveSet[ThreadNumber] = NewNode;
				ActiveJunctions[ThreadNumber]++;
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


// Calculate the position of the dynamic boundary
void CalculateBoundary(void)
{
	int OldBoundary = xMax[0];

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
	}
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
void CopyNodeAdditions(ActiveNode **pHead, ActiveNode **pAdditionsHead, int *nActive)
{
	ActiveNode *TempNode, *CurrentNode, *PreviousNode;

	PreviousNode = NULL;
	CurrentNode = *pAdditionsHead;
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
				*pAdditionsHead = CurrentNode;
			}
		}
		else {
			(*nActive)++;
			Grid[CurrentNode->X][CurrentNode->Y][CurrentNode->Z].Active = true;
			// Get the next node in the list
			PreviousNode = CurrentNode;
			CurrentNode = CurrentNode->NextActiveNode;
		}
	}

	// Add the list to the head of the main list
	if (*pAdditionsHead != NULL) {
		if (PreviousNode != NULL) {
			PreviousNode->NextActiveNode = *pHead;
		}
		else {
			(*pAdditionsHead)->NextActiveNode = *pHead;
		}
		*pHead = *pAdditionsHead;
		*pAdditionsHead = NULL;
	}		
}


// Remove nodes no longer in set 1
void CorrectActiveSet(int Set1) 
{
	ActiveNode *CurrentNode, *PreviousNode;
	int Set2 = 1-Set1;

	PreviousNode = NULL;
	CurrentNode = ActiveSet[Set1];

	while (CurrentNode != NULL) {
		if (CurrentNode->X > xMax[Set1] || CurrentNode->X < xMin[Set1]) {
			// In the middle of the list
			if (PreviousNode != NULL) {
				PreviousNode->NextActiveNode = CurrentNode->NextActiveNode;
				CurrentNode->NextActiveNode = ActiveSet[Set2];
				ActiveSet[Set2] = CurrentNode;
				CurrentNode = PreviousNode->NextActiveNode;
			}
			else {
				// At the start of the list
				ActiveSet[Set1] = CurrentNode->NextActiveNode;
				CurrentNode->NextActiveNode = ActiveSet[Set2];
				ActiveSet[Set2] = CurrentNode;
				CurrentNode = ActiveSet[Set1];
			}
			ActiveJunctions[Set2]++;
			ActiveJunctions[Set1]--;
		}
		else {
			PreviousNode = CurrentNode;
			CurrentNode = CurrentNode->NextActiveNode;
		}
	}
}