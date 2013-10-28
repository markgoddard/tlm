/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		17/10/2008
//	File:		TLMMaths.h
//
/*********************************************************************************************/

#ifndef TLM_MATHS_H
#define TLM_MATHS_H

// Definitions
#define IMPEDANCE_OF_FREE_SPACE	376.73031346177
#define SPEED_OF_LIGHT 299792458

// For use with line segments
#define LINE_CONST 0
#define LINE_PARAM 1

// Inline macros
#define MIN(a,b)	( ((a)>(b)) ? (b) : (a) )
#define MAX(a,b)	( ((a)<(b)) ? (b) : (a) )
#define SQUARE(a)	( (a) * (a) )

// Type definitions

// Coordinate container
typedef struct {
				float X;
				float Y;
				float Z;
				} Coordinate;

typedef struct {
				float X;
				float Y;
				} xyCoordinate;

// Line segment container
typedef struct {
				Coordinate Constant;
				Coordinate Parameter;
				} LineSegment;

typedef struct {
				float xCoeff;
				float yCoeff;
				float Constant;
				} xyLine;

// Function prototypes

// Conversion between units
float VoltageToDB(float Voltage);

// Round floating point numbers
int RoundUpwards(float Number);
int RoundToNearest(float Number);

// Geometry functions
xyCoordinate CoordinateToXY(Coordinate P);
xyLine CoordinatesToLine(xyCoordinate P1, xyCoordinate P2);
float XFromY(xyLine L, float Y);
float YFromX(xyLine L, float X);
float DistanceFromLine(xyLine L, xyCoordinate P);
float SineThetaFromLine(xyLine L);
float CosineThetaFromLine(xyLine L);
xyCoordinate xyIntersection(xyLine L1, xyLine L2);
xyLine xyNormal(xyLine L, xyCoordinate P);
float AngleBetweenLines(xyLine L1, xyLine L2);
bool IsWithinTriangle(xyLine L1, xyLine L2, xyLine L3, xyCoordinate P, int Direction);


#endif //TLM_MATHS_H