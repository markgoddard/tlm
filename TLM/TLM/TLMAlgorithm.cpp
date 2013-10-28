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


// Global variables
static int ActiveJunctions;
static double AbsoluteThreshold;


extern Node ***Grid;
extern TimeVariationSet *TimeVariation;
extern int xSize, ySize, zSize;
extern Source ImpulseSource;
extern int MaxIterations;
extern InputFlags InputData;

extern double Frequency;
extern double MaxPathLoss;
extern double RelativeThreshold;
extern double GridSpacing;


// Function prototypes
void SingleIteration(ActiveNode **ActiveSet);
void EvaluateSource(int Iteration);
ActiveNode *AddJunctionToSet(int x, int y, int z);
ActiveNode *RemoveJunctionFromSet(int x, int y, int z, ActiveNode *InactiveNode);


// Top level loop for TLM algorithm
void MainLoop(void)
{
	ActiveNode *ActiveSet;
	TimeVariationSet *CurrentTimeVariation = NULL;
	int nIterations = 0;

	// Calculate the absolute threshold from the path loss
	AbsoluteThreshold = 4*M_PI*GridSpacing*pow(10, MaxPathLoss/20)/KAPPA*Frequency/SPEED_OF_LIGHT;

	// Add the impulse junction to the active set
	ActiveSet = AddJunctionToSet(ImpulseSource.X, ImpulseSource.Y, ImpulseSource.Z);
	ActiveJunctions = 1;

	// Repeat the algorithm while the active set is not empty
	while (ActiveSet != NULL) {
		// Evaluate source output
		if (nIterations < ImpulseSource.Duration) {
			EvaluateSource(nIterations);
		}
		
		// Print a portion of the grid
		if (InputData.PrintTimeVariation.Flag == true) {
			if (CurrentTimeVariation == NULL) {
				TimeVariation = (TimeVariationSet*)malloc(sizeof(TimeVariationSet));
				CurrentTimeVariation = TimeVariation;
			}
			else {
				CurrentTimeVariation->NextSet = (TimeVariationSet*)malloc(sizeof(TimeVariationSet));
				CurrentTimeVariation = CurrentTimeVariation->NextSet;
			}
			CurrentTimeVariation->NextSet = NULL;			
			SaveTimeVariation(CurrentTimeVariation);
		}

		// Perform a single iteration of the algorithm
		SingleIteration(&ActiveSet);

		// Increment the number of iterations completed
		nIterations++;

		printf("Completed %d iterations, %d active junctions\n", nIterations, ActiveJunctions);

	}
	printf("Algorithm complete, took %d iterations\n", nIterations);
}


// Single iteration of the TLM algorithm
void SingleIteration(ActiveNode **pActiveSet) 
{
	double Value;			// Temporary node value
	Node *NodeReference;	// Temporary node reference
	ActiveNode *CurrentNode;
	ActiveNode *PreviousNode;
	int x, y, z;

	// Setup the current node pointer
	CurrentNode = *pActiveSet;

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
		if (x < (xSize - 1)) {
			if (Grid[x+1][y][z].Active == false && Grid[x+1][y][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x+1,y,z);
				NewNode->NextActiveNode = *pActiveSet;
				*pActiveSet = NewNode;
				ActiveJunctions++;
			}
		}
		// Negative x direction
		if (x > 0) {
			if (Grid[x-1][y][z].Active == false && Grid[x-1][y][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x-1,y,z);
				NewNode->NextActiveNode = *pActiveSet;
				*pActiveSet = NewNode;
				ActiveJunctions++;
			}
		}
		// Positive y direction
		if (y < (ySize - 1)) {
			if (Grid[x][y+1][z].Active == false && Grid[x][y+1][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y+1,z);
				NewNode->NextActiveNode = *pActiveSet;
				*pActiveSet = NewNode;
				ActiveJunctions++;
			}
		}
		// Negative y direction
		if (y > 0) {
			if (Grid[x][y-1][z].Active == false && Grid[x][y-1][z].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y-1,z);
				NewNode->NextActiveNode = *pActiveSet;
				*pActiveSet = NewNode;
				ActiveJunctions++;
			}
		}
		// Positive z direction
		if (z < (zSize - 1)) {
			if (Grid[x][y][z+1].Active == false && Grid[x][y][z+1].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y,z+1);
				NewNode->NextActiveNode = *pActiveSet;
				*pActiveSet = NewNode;
				ActiveJunctions++;
			}
		}
		// Negative y direction
		if (z > 0) {
			if (Grid[x][y][z-1].Active == false && Grid[x][y][z-1].PropagateFlag == true) {
				ActiveNode *NewNode;

				NewNode = AddJunctionToSet(x,y,z-1);
				NewNode->NextActiveNode = *pActiveSet;
				*pActiveSet = NewNode;
				ActiveJunctions++;
			}
		}
		// Get the next node in the active set
		CurrentNode = CurrentNode->NextActiveNode;
	}

	// Connect phase, compute the junction inputs for all of the junctions in the active set
	PreviousNode = NULL;
	CurrentNode = *pActiveSet;

	while (CurrentNode != NULL) {

		// Get local copies of the position variables and the node pointer
		x = CurrentNode->X;
		y = CurrentNode->Y;
		z = CurrentNode->Z;
		NodeReference = &Grid[x][y][z];

		// Positive x-direction
		NodeReference->VxpIn = NodeReference->Rxp*NodeReference->VxpOut;
		if (x < (xSize-1)) {
			NodeReference->VxpIn += Grid[x+1][y][z].VxnOut * NodeReference->Txp;
		}

		// Negative x-direction
		NodeReference->VxnIn = NodeReference->Rxn*NodeReference->VxnOut;
		if (x > 0) {
			NodeReference->VxnIn += Grid[x-1][y][z].VxpOut * NodeReference->Txn;
		}

		// Positive y-direction
		NodeReference->VypIn = NodeReference->Ryp*NodeReference->VypOut;
		if (y < (ySize-1)) {
			NodeReference->VypIn += Grid[x][y+1][z].VynOut * NodeReference->Typ;
		}

		// Negative y-direction
		NodeReference->VynIn = NodeReference->Ryn*NodeReference->VynOut;
		if (y > 0) {
			NodeReference->VynIn += Grid[x][y-1][z].VypOut * NodeReference->Tyn;
		}

		// Positive z-direction
		NodeReference->VzpIn = NodeReference->Rzp*NodeReference->VzpOut;
		if (z < (zSize-1)) {
			NodeReference->VzpIn += Grid[x][y][z+1].VznOut * NodeReference->Tzp;
		}

		// Negative z-direction
		NodeReference->VznIn = NodeReference->Rzn*NodeReference->VznOut;
		if (z > 0) {
			NodeReference->VznIn += Grid[x][y][z-1].VzpOut * NodeReference->Tzn;
		}

		// Compute the state of the node
		Value = NodeReference->VxpIn + 
				NodeReference->VxnIn +
				NodeReference->VypIn +
				NodeReference->VynIn +
				NodeReference->VzpIn +
				NodeReference->VznIn;

		// Check if this is the highest value that has been seen at this node
		if (abs(Value) > Grid[x][y][z].Vmax) {
			Grid[x][y][z].Vmax = abs(Value);
		}

		// Assign to the node
		Grid[x][y][z].V = Value;

		if (abs(Value) < AbsoluteThreshold || abs(Value) < NodeReference->Vmax*RelativeThreshold) {
			CurrentNode = RemoveJunctionFromSet(x,y,z,CurrentNode);
			ActiveJunctions--;
			if (PreviousNode != NULL) {
				PreviousNode->NextActiveNode = CurrentNode;
			}
			else {
				*pActiveSet = CurrentNode;
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

	Grid[ImpulseSource.X][ImpulseSource.Y][ImpulseSource.Z].V = V;

	if (abs(V) > Grid[ImpulseSource.X][ImpulseSource.Y][ImpulseSource.Z].Vmax) {
		Grid[ImpulseSource.X][ImpulseSource.Y][ImpulseSource.Z].Vmax = abs(V);
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

	return NextNode;
}