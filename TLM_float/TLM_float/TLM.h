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
					float V;
					// Input variables 
					float VxpIn,
						   VxnIn,
						   VypIn,
						   VynIn,
						   VzpIn,
						   VznIn;
					// Output variables
					float VxpOut,
						   VxnOut,
						   VypOut,
						   VynOut,
						   VzpOut,
						   VznOut;
					// Peak voltage observed
					float Vmax;
					// Node properties
					float Z;
					// Reflection coefficients
					float Rxp,
						   Rxn,
						   Ryp,
						   Ryn,
						   Rzp,
						   Rzn;
					// Transmission coefficients
					float Txp,
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
							float *V;
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
				float X1;
				float Y1;
				float X2;
				float Y2;
				float Z1;
				float Z2;
				float Spacing;
				float SpacingY;
				float SpacingZ;
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