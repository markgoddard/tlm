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

// Averaging method

// The path loss constant
#define KAPPA 1.32

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
				double X;
				double Y;
				double Z;
				} Coordinate;

typedef struct {
				double X;
				double Y;
				} xyCoordinate;

// Line segment container
typedef struct {
				Coordinate Constant;
				Coordinate Parameter;
				} LineSegment;

typedef struct {
				double xCoeff;
				double yCoeff;
				double Constant;
				} xyLine;

// Function prototypes

// Conversion between units
double VoltageToDB(double Voltage);

// Round floating point numbers
int RoundUpwards(double Number);
int RoundToNearest(double Number);

// Geometry functions
xyCoordinate CoordinateToXY(Coordinate P);
xyLine CoordinatesToLine(xyCoordinate P1, xyCoordinate P2);
double XFromY(xyLine L, double Y);
double YFromX(xyLine L, double X);
double DistanceFromLine(xyLine L, xyCoordinate P);
double SineThetaFromLine(xyLine L);
double CosineThetaFromLine(xyLine L);
xyCoordinate xyIntersection(xyLine L1, xyLine L2);
xyLine xyNormal(xyLine L, xyCoordinate P);
double AngleBetweenLines(xyLine L1, xyLine L2);
bool IsWithinTriangle(xyLine L1, xyLine L2, xyLine L3, xyCoordinate P, int Direction);


#endif //TLM_MATHS_H