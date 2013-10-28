/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		20/10/2008
//	File:		TLM.h
//
/*********************************************************************************************/

#ifndef TLM_H
#define TLM_H

// Type definitions

// The type of source required
enum SourceType {
				 IMPULSE,
				 GAUSSIAN,
				 RAISED_COSINE
				};


// Structure to hold a single node
typedef struct {	// Current state
					double V;
					// Input variables 
					double VxpIn,
						   VxnIn,
						   VypIn,
						   VynIn,
						   VzpIn,
						   VznIn;
					// Output variables
					double VxpOut,
						   VxnOut,
						   VypOut,
						   VynOut,
						   VzpOut,
						   VznOut;
					// Peak voltage observed
					double Epulse;
					double Em;
					double Emm;
					double Emax;
					double Em_max;
					double Emm_max;
					// Node properties
					double Z;
					// Reflection coefficients
					double Rxp,
						   Rxn,
						   Ryp,
						   Ryn,
						   Rzp,
						   Rzn;
					// Transmission coefficients
					double Txp,
						   Txn,
						   Typ,
						   Tyn,
						   Tzp,
						   Tzn;
					bool PropagateFlag;
					bool Active;
				} Node;


// Structure to hold the details of the source
typedef struct {
				SourceType	Type;
				// Position in the grid
				int			X,
							Y,
							Z;
				// Duration of source
				int			Duration;
				} Source;


// Structure to hold the details of an active node as part of a linked list
struct ActiveNode {
					int X,
						Y,
						Z;
					ActiveNode *NextActiveNode;
					};


// Structure to hold a set of time variation values
struct TimeVariationSet {
							double *V;
							TimeVariationSet *NextSet;
						};


// Structure to hold a boolean input parameter
typedef struct {
				bool Flag;
				bool Default;
				} InputFlag;

typedef struct {
				InputFlag DisplayPolygonInformation;
				InputFlag PrintTimeVariation;
				InputFlag PrintTimingInformation;
				} InputFlags;


// Enumeration to store the type of path loss sweep required
typedef enum {
				NONE,
				POINT,
				ROUTE,
				GRID,
				CUBE
				} PLType;


// Structure to hold path loss point, route or grid info
typedef struct {
				PLType Type;
				double X1;
				double Y1;
				double X2;
				double Y2;
				double Z1;
				double Z2;
				double Spacing;
				double SpacingY;
				double SpacingZ;
				} PLParams;


typedef struct {
				clock_t StartTime;
				clock_t SceneParsingStartTime;
				clock_t SceneParsingFinishTime;
				clock_t AlgorithmStartTime;
				clock_t AlgorithmFinishTime;
				clock_t FinishTime;
				} TimingInformation;

#endif	// TLM_H