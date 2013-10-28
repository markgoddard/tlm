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
//#define AVERAGE
#define RMS
//#define IIR
//#define RMS_IIR
//#define POWER

// The path loss constant
#ifdef AVERAGE
#define KAPPA 4.5
#endif // AVERAGE
#ifdef RMS
#define KAPPA 4.5
#endif // RMS
#ifdef IIR
#define KAPPA 7.134
#endif // IIR
#ifdef RMS_IIR
#define KAPPA 8.755
#endif // RMS_IIR
#ifdef POWER
#define KAPPA 1
#endif // POWER

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