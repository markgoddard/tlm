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
						UNFINISHED,
						A_FIRST,
						B_FIRST,
						END
						} flag_t;


// Global variables
static int ActiveJunctionsA, ActiveJunctionsB;
double AbsoluteThreshold;

static ActiveNode *ActiveSetA = NULL;
static ActiveNode *ActiveSetB = NULL;
static ActiveNode *NodeAdditionsA = NULL;
static ActiveNode *NodeAdditionsB = NULL;
static HANDLE hMutexA;
static HANDLE hMutexB;
static HANDLE hMutexFlag;
static flag_t fFlag = UNFINISHED;

extern Node ***Grid;
extern int xSize, ySize, zSize;
extern Source ImpulseSource;
extern InputFlags InputData;

extern double Frequency;
extern double MaxPathLoss;
extern double RelativeThreshold;
extern double GridSpacing;


// Function prototypes
DWORD WINAPI SecondaryThread(LPVOID *lpParam);
void ScatterA(void);
void ScatterB(void);
void ConnectA(void);
void ConnectB(void);
void EvaluateSource(int Iteration);
ActiveNode *AddJunctionToSet(int x, int y, int z, bool Active);
ActiveNode *RemoveJunctionFromSet(int x, int y, int z, ActiveNode *InactiveNode);
ActiveNode *CopyNodeAdditions(ActiveNode *Head, ActiveNode **pAdditionsHead, int *nActive);




// Top level loop for TLM algorithm
void MainLoop(void)
{
	int nIterations = 0;
	HANDLE hSecondaryThread;
	DWORD dwSecondaryThreadID;
	flag_t Flag;

	// Calculate the absolute threshold from the path loss
	AbsoluteThreshold = SQUARE(4*M_PI*GridSpacing/KAPPA*Frequency/SPEED_OF_LIGHT)*pow(10, MaxPathLoss/10.0);
	RelativeThreshold *= RelativeThreshold;

	// Add the impulse junction to the active set
	if (ImpulseSource.X < xSize/2) {
		ActiveJunctionsA = 1;
		ActiveJunctionsB = 0;
		ActiveSetA = AddJunctionToSet(ImpulseSource.X, ImpulseSource.Y, ImpulseSource.Z, true);
	}
	else {
		ActiveJunctionsA = 0;
		ActiveJunctionsB = 1;
		ActiveSetB = AddJunctionToSet(ImpulseSource.X, ImpulseSource.Y, ImpulseSource.Z, true);
	}

	// Create the mutex objects, initially own mutex A
	hMutexA = CreateMutex(NULL, TRUE, NULL);
	hMutexB = CreateMutex(NULL, TRUE, NULL);
	hMutexFlag = CreateMutex(NULL, FALSE, NULL);

	// Start the secondary thread
	hSecondaryThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SecondaryThread, NULL, 0, &dwSecondaryThreadID);
	if (hSecondaryThread == NULL) {
		printf("Secondary thread could not be started\n");
		exit(1);
	}
	else {
		printf("Secondary thread started\n");
	}

	// Repeat the algorithm while the active set is not empty
	while (ActiveSetA != NULL || ActiveSetB != NULL) {
		
		// Evaluate source output
		if (nIterations < ImpulseSource.Duration) {
			EvaluateSource(nIterations);
		}

		ReleaseMutex(hMutexB);

		// Perform a single iteration of the scatter algorithm
		ScatterA();

		// Check the flag to see if B has finished yet
		WaitForSingleObject(hMutexFlag, INFINITE);
		Flag = fFlag;
		if (Flag == UNFINISHED) {
			fFlag = A_FIRST;
		}
		else {
			fFlag = UNFINISHED;
		}
		ReleaseMutex(hMutexFlag);

		if (Flag == UNFINISHED) {
			WaitForSingleObject(hMutexB, INFINITE);
			ReleaseMutex(hMutexA);
		}
		else {
			ReleaseMutex(hMutexA);
			WaitForSingleObject(hMutexB, INFINITE);
		}

		// Add nodes on boundaries to lists
		ActiveSetA = CopyNodeAdditions(ActiveSetA, &NodeAdditionsA, &ActiveJunctionsA);

		// Begin the connect phase
		ConnectA();

		// Check the flag to see if B has finished yet
		WaitForSingleObject(hMutexFlag, INFINITE);
		Flag = fFlag;
		if (Flag == UNFINISHED) {
			fFlag = A_FIRST;
		}
		else {
			fFlag = UNFINISHED;
		}
		ReleaseMutex(hMutexFlag);

		if (Flag == UNFINISHED) {
			WaitForSingleObject(hMutexA, INFINITE);
		}
		else {
			ReleaseMutex(hMutexB);
			WaitForSingleObject(hMutexA, INFINITE);
		}

		// Increment the number of iterations completed
		nIterations++;

		printf("Completed %d iterations, %d active junctions in A, %d in B, %d total\n", nIterations, ActiveJunctionsA, ActiveJunctionsB, ActiveJunctionsA+ActiveJunctionsB);
	}

	printf("Algorithm complete, took %d iterations\n", nIterations);
}


// Secondary Thread Function
DWORD WINAPI SecondaryThread(LPVOID *lpParam)
{
	flag_t Flag;

	WaitForSingleObject(hMutexB, INFINITE);

	while (1) {
		ScatterB();
		// Check the flag to see if A has finished yet
		WaitForSingleObject(hMutexFlag, INFINITE);
		Flag = fFlag;
		if (Flag == UNFINISHED) {
			fFlag = B_FIRST;
		}
		else {
			fFlag = UNFINISHED;
		}
		ReleaseMutex(hMutexFlag);

		if (Flag == END) {
			ExitThread(0);
		}
		else if (Flag == UNFINISHED) {
			WaitForSingleObject(hMutexA, INFINITE);
			ReleaseMutex(hMutexB);
		}
		else {
			ReleaseMutex(hMutexB);
			WaitForSingleObject(hMutexA, INFINITE);
		}
		
		// Add nodes to the main active list
		ActiveSetB = CopyNodeAdditions(ActiveSetB, &NodeAdditionsB, &ActiveJunctionsB);
		
		// Connect phase
		ConnectB();
		
		// Check the flag to see if A has finished yet
		WaitForSingleObject(hMutexFlag, INFINITE);
		Flag = fFlag;
		if (Flag == UNFINISHED) {
			fFlag = B_FIRST;
		}
		else {
			fFlag = UNFINISHED;
		}
		ReleaseMutex(hMutexFlag);

		if (Flag == UNFINISHED) {
			WaitForSingleObject(hMutexB, INFINITE);
			ReleaseMutex(hMutexA);
		}
		else {
			ReleaseMutex(hMutexA);
			WaitForSingleObject(hMutexB, INFINITE);
		}
	}
}


// Single iteration of the TLM algorithm scatter sequence
void ScatterA(void) 
{
	double Value;			// Temporary node value
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	int x, y, z;
	int xMax = xSize/2-1;

	// Setup the current node pointer
	CurrentNode = ActiveSetA;

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
		if (x < xMax) {
			if (Grid[x+1][y][z].Active == false && Grid[x+1][y][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x+1,y,z, true);
				NewNode->NextActiveNode = ActiveSetA;
				ActiveSetA = NewNode;
				ActiveJunctionsA++;
			}
		}
		// Boundary check
		if (x == xMax) {
			if (Grid[x+1][y][z].Active == false && Grid[x+1][y][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x+1,y,z, false);
				NewNode->NextActiveNode = NodeAdditionsB;
				NodeAdditionsB = NewNode;
			}
		}
		// Negative x direction
		if (x > 0) {
			if (Grid[x-1][y][z].Active == false && Grid[x-1][y][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x-1,y,z, true);
				NewNode->NextActiveNode = ActiveSetA;
				ActiveSetA = NewNode;
				ActiveJunctionsA++;
			}
		}
		// Positive y direction
		if (y < (ySize - 1)) {
			if (Grid[x][y+1][z].Active == false && Grid[x][y+1][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y+1,z, true);
				NewNode->NextActiveNode = ActiveSetA;
				ActiveSetA = NewNode;
				ActiveJunctionsA++;
			}
		}
		// Negative y direction
		if (y > 0) {
			if (Grid[x][y-1][z].Active == false && Grid[x][y-1][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y-1,z, true);
				NewNode->NextActiveNode = ActiveSetA;
				ActiveSetA = NewNode;
				ActiveJunctionsA++;
			}
		}
		// Positive z direction
		if (z < (zSize - 1)) {
			if (Grid[x][y][z+1].Active == false && Grid[x][y][z+1].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y,z+1, true);
				NewNode->NextActiveNode = ActiveSetA;
				ActiveSetA = NewNode;
				ActiveJunctionsA++;
			}
		}
		// Negative y direction
		if (z > 0) {
			if (Grid[x][y][z-1].Active == false && Grid[x][y][z-1].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y,z-1, true);
				NewNode->NextActiveNode = ActiveSetA;
				ActiveSetA = NewNode;
				ActiveJunctionsA++;
			}
		}
		// Get the next node in the active set
		CurrentNode = CurrentNode->NextActiveNode;
	}
}


// Single iteration of the TLM algorithm connent sequence
void ConnectA(void) 
{
	double Value;			// Temporary node value
	double AvgEnergy;		// Average energy over two iterations
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *PreviousNode;
	int x, y, z;

	// Connect phase, compute the junction inputs for all of the junctions in the active set
	PreviousNode = NULL;
	CurrentNode = ActiveSetA;

	while (CurrentNode != NULL) {

		// Get local copies of the position variables and the node pointer
		x = CurrentNode->X;
		y = CurrentNode->Y;
		z = CurrentNode->Z;
		NodeReference = &Grid[x][y][z];

		// Check if the node is a material or grid boundary, if so then incorporate transmission and reflection coefficients
		if (Grid[x][y][z].RT == NULL) {
			NodeReference->VxpIn = Grid[x+1][y][z].VxnOut;
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
			NodeReference->VxpIn += Grid[x+1][y][z].VxnOut * RT->Txp;

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
			ActiveJunctionsA--;
			if (PreviousNode != NULL) {
				PreviousNode->NextActiveNode = CurrentNode;
			}
			else {
				ActiveSetA = CurrentNode;
			}
		}
		else {
			// Get the next active node from the list
			PreviousNode = CurrentNode;
			CurrentNode = CurrentNode->NextActiveNode;
		}
	}
}



// Single iteration of the TLM algorithm scatter sequence
void ScatterB(void) 
{
	double Value;			// Temporary node value
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	int x, y, z;
	int xMin = xSize/2;

	// Setup the current node pointer
	CurrentNode = ActiveSetB;

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
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x+1,y,z, true);
				NewNode->NextActiveNode = ActiveSetB;
				ActiveSetB = NewNode;
				ActiveJunctionsB++;
			}
		}
		// Negative x direction
		if (x > xMin) {
			if (Grid[x-1][y][z].Active == false && Grid[x-1][y][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x-1,y,z, true);
				NewNode->NextActiveNode = ActiveSetB;
				ActiveSetB = NewNode;
				ActiveJunctionsB++;
			}
		}
		// On the boundary
		if (x == xMin) {
			if (Grid[x-1][y][z].Active == false && Grid[x-1][y][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x-1,y,z, false);
				NewNode->NextActiveNode = NodeAdditionsA;
				NodeAdditionsA = NewNode;
			}
		}
		// Positive y direction
		if (y < (ySize - 1)) {
			if (Grid[x][y+1][z].Active == false && Grid[x][y+1][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y+1,z, true);
				NewNode->NextActiveNode = ActiveSetB;
				ActiveSetB = NewNode;
				ActiveJunctionsB++;
			}
		}
		// Negative y direction
		if (y > 0) {
			if (Grid[x][y-1][z].Active == false && Grid[x][y-1][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y-1,z, true);
				NewNode->NextActiveNode = ActiveSetB;
				ActiveSetB = NewNode;
				ActiveJunctionsB++;
			}
		}
		// Positive z direction
		if (z < (zSize - 1)) {
			if (Grid[x][y][z+1].Active == false && Grid[x][y][z+1].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y,z+1, true);
				NewNode->NextActiveNode = ActiveSetB;
				ActiveSetB = NewNode;
				ActiveJunctionsB++;
			}
		}
		// Negative y direction
		if (z > 0) {
			if (Grid[x][y][z-1].Active == false && Grid[x][y][z-1].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y,z-1, true);
				NewNode->NextActiveNode = ActiveSetB;
				ActiveSetB = NewNode;
				ActiveJunctionsB++;
			}
		}
		// Get the next node in the active set
		CurrentNode = CurrentNode->NextActiveNode;
	}
}


// Single iteration of the TLM algorithm connent sequence
void ConnectB(void) 
{
	double Value;			// Temporary node value
	double AvgEnergy;		// Average energy over two iterations
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *PreviousNode;
	int x, y, z;

	// Connect phase, compute the junction inputs for all of the junctions in the active set
	PreviousNode = NULL;
	CurrentNode = ActiveSetB;

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
			else {
				NodeReference->VxpIn = 0;
			}
			NodeReference->VxnIn = Grid[x-1][y][z].VxpOut;

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
			NodeReference->VxnIn += Grid[x-1][y][z].VxpOut * RT->Txn;

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
			ActiveJunctionsB--;
			if (PreviousNode != NULL) {
				PreviousNode->NextActiveNode = CurrentNode;
			}
			else {
				ActiveSetB = CurrentNode;
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
ActiveNode *CopyNodeAdditions(ActiveNode *Head, ActiveNode **pAdditionsHead, int *nActive)
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
			PreviousNode->NextActiveNode = Head;
		}
		else {
			(*pAdditionsHead)->NextActiveNode = Head;
		}
		Head = *pAdditionsHead;
		*pAdditionsHead = NULL;
	}		

	return Head;
}