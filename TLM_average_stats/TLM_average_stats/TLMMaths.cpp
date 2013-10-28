/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		17/10/2008
//	File:		TLMMaths.cpp
//
/*********************************************************************************************/

// Header files
#include "stdafx.h"
#include "TLMMaths.h"

// The value of the noise floor requested by the user
extern double MaxPathLoss;

// Convert a voltage in Volts to a power in dB
double VoltageToDB(double Voltage)
{
	double Power;
	
	if (Voltage == 0) {
		Power = MaxPathLoss;
	}
	else {
		Power = 20*log10(Voltage);
	}

	return Power;
}

// Round a floating point number up to the nearest integer (ceiling function)
int RoundUpwards(double Number)
{
	int iNumber = (int) Number;
	if (Number >= 0) {
		return Number == iNumber ? iNumber : iNumber + 1;
	}
	else {
		return iNumber;
	}
}


// Round a floating point number to the nearest integer 
int RoundToNearest(double Number)
{
	int iNumber = (int) Number;
	double Decimal = Number - iNumber;

	return Decimal <= 0.5 ? iNumber : iNumber + 1;
}



// Convert a 3D coordinate to a 2D xyCoordinate
xyCoordinate CoordinateToXY(Coordinate P)
{
	xyCoordinate xyP;

	xyP.X = P.X;
	xyP.Y = P.Y;

	return xyP;
}


// Construct an equation of a line of the form	ax + by + c = 0 from two points in the xy plane
xyLine CoordinatesToLine(xyCoordinate P1, xyCoordinate P2)
{
	xyLine L;
	double dy, dx;

	dy = P2.Y - P1.Y;
	dx = P2.X - P1.X;

	L.xCoeff = -dy;
	L.yCoeff = dx;
	L.Constant = P1.X*dy - P1.Y*dx;
	
	return L;
}


double XFromY(xyLine L, double Y)
{
	double X;

	X = -(Y*L.yCoeff+L.Constant)/L.xCoeff; 

	return X;
}


double YFromX(xyLine L, double X)
{
	double Y;

	Y = -(X*L.xCoeff+L.Constant)/L.yCoeff; 

	return Y;
}


// Determine whether a point lies on a line
double DistanceFromLine(xyLine L, xyCoordinate P)
{
	double Result;

	Result = (L.xCoeff*P.X + L.yCoeff*P.Y + L.Constant)/sqrt(SQUARE(L.xCoeff)+SQUARE(L.yCoeff));

	return Result;
}


double SineThetaFromLine(xyLine L)
{
	double SineTheta;

	SineTheta = -L.xCoeff/sqrt((SQUARE(L.yCoeff)+SQUARE(L.xCoeff)));

	return SineTheta;
}


double CosineThetaFromLine(xyLine L)
{
	double CosineTheta;

	CosineTheta = L.yCoeff/sqrt((SQUARE(L.yCoeff)+SQUARE(L.xCoeff)));

	return CosineTheta;
}


xyCoordinate xyIntersection(xyLine L1, xyLine L2)
{
	xyCoordinate Intersection;
	double Denom;
	double xNumerator, yNumerator;

	Denom = L1.xCoeff*L2.yCoeff - L1.yCoeff*L2.xCoeff;

	// Check to see if there is an intersection
	if (Denom == 0) {
	}
	else {
		xNumerator = L1.yCoeff*L2.Constant - L1.Constant*L2.yCoeff;
		yNumerator = L1.xCoeff*L2.Constant - L1.Constant*L2.xCoeff;
		Intersection.X = xNumerator/Denom;
		Intersection.Y = yNumerator/Denom;
	}

	return Intersection;
}


xyLine xyNormal(xyLine L, xyCoordinate P)
{
	xyLine Normal;

	Normal.xCoeff = L.yCoeff;
	Normal.yCoeff = -L.xCoeff;
	Normal.Constant = -Normal.xCoeff*P.X - Normal.yCoeff*P.Y;

	return Normal;
}


double AngleBetweenLines(xyLine L1, xyLine L2)
{
	double SineTheta;
	double Angle;

	SineTheta = (L1.xCoeff*L2.yCoeff - L1.yCoeff*L2.xCoeff)/sqrt((SQUARE(L1.xCoeff)+SQUARE(L1.yCoeff))*(SQUARE(L2.xCoeff)+SQUARE(L2.yCoeff)));
	if (SineTheta > 1) {
		SineTheta = 1;
	}
	else if (SineTheta < -1) {
		SineTheta = -1;
	}
	Angle = asin(SineTheta);

	return Angle;
}

// For a triangle specified by three lines (L1 = L2 + L3 in vector form)
bool IsWithinTriangle(xyLine L1, xyLine L2, xyLine L3, xyCoordinate P, int Direction)
{
	bool IsWithin = true;
	double Result;

	Result = L1.xCoeff*P.X + L1.yCoeff*P.Y + L1.Constant;
	if (Result * Direction > 0) {
		IsWithin = false;
	}
	Result = L2.xCoeff*P.X + L2.yCoeff*P.Y + L2.Constant;
	if (Result * Direction < 0) {
		IsWithin = false;
	}
	Result = L3.xCoeff*P.X + L3.yCoeff*P.Y + L3.Constant;
	if (Result * Direction < 0) {
		IsWithin = false;
	}

	return IsWithin;
}