// Add a vertical polygon to the TLM grid
void AddVerticalPolygon(Polygon *VPolygon, double Thickness, double Permittivity, bool PropagateFlag)
{
	xyCoordinate P1 = {VPolygon->Vertices[0].X, VPolygon->Vertices[0].Y};
	xyCoordinate P2 = {VPolygon->Vertices[1].X, VPolygon->Vertices[1].Y};
	xyCoordinate Vertices[4];
	xyLine Sides[4];
	xyLine CentralLine;
	double SineTheta, CosineTheta;
	double dx, dy;
	double Impedance;
	int xMin, xMax;
	int yMin, yMax;
	int zMin, zMax;

	// Set the impedance variable
	Impedance = IMPEDANCE_OF_FREE_SPACE/sqrt(Permittivity);

	// Find the minimum and maximum nodes in the z-direction
	if (VPolygon->Vertices[0].Z < VPolygon->Vertices[1].Z) {
		zMin = RoundUpwards(VPolygon->Vertices[0].Z/GridSpacing);
		zMax = (int)(VPolygon->Vertices[1].Z/GridSpacing);
	}
	else {
		zMin = RoundUpwards(VPolygon->Vertices[1].Z/GridSpacing);
		zMax = (int)(VPolygon->Vertices[0].Z/GridSpacing);
	}
	zMin = PlaceWithinGridZ(zMin);
	zMax = PlaceWithinGridZ(zMax);

	// Ensure the smallest X-coordinate is in P1
	if (P1.X > P2.X) {
		// Swap the coordinates
		xyCoordinate P = P1;
		P1 = P2;
		P2 = P;
	}
	// For vertical lines ensure the first y coordinate is greatest
	else if (P1.X == P2.X) {
		if (P1.Y < P2.Y) {
			// Swap the coordinates
			xyCoordinate P = P1;
			P1 = P2;
			P2 = P;
		}
	}

	// Find the equation of the line connecting the two points
	CentralLine = CoordinatesToLine(P1,P2);
	SineTheta = SineThetaFromLine(CentralLine);
	CosineTheta = CosineThetaFromLine(CentralLine);

	// Ensure the rectangle is at least as wide as the grid spacing
	if (Thickness < GridSpacing) {
		Thickness = GridSpacing;
	}

	// Find the vertices of the rectangle
	dx = abs(Thickness*SineTheta/2);
	dy = abs(Thickness*CosineTheta/2);

	// Find the vertices of the rectangle

	// Outer most x terms always the same
	Vertices[0].X = P1.X - dx;
	Vertices[3].X = P2.X + dx;

	// Check the gradient of the line
	if (CentralLine.xCoeff*CentralLine.yCoeff < 0 || CentralLine.xCoeff == 0) {
		// Positive or zero gradient
		Vertices[0].Y = P1.Y + dy;
		Vertices[1].X = P1.X + dx;
		Vertices[1].Y = P1.Y - dy;
		Vertices[2].X = P2.X - dx;
		Vertices[2].Y = P2.Y + dy;
		Vertices[3].Y = P2.Y - dy;
	}
	else {
		// Negative or infinite gradient
		Vertices[0].Y = P1.Y - dy;
		Vertices[1].X = P2.X - dx;
		Vertices[1].Y = P2.Y - dy;
		Vertices[2].X = P1.X + dx;
		Vertices[2].Y = P1.Y + dy;
		Vertices[3].Y = P2.Y + dy;
	}
	
	// Find the sides of the rectangle
	Sides[0] = CoordinatesToLine(Vertices[0],Vertices[1]);
	Sides[1] = CoordinatesToLine(Vertices[0],Vertices[2]);
	Sides[2] = CoordinatesToLine(Vertices[1],Vertices[3]);
	Sides[3] = CoordinatesToLine(Vertices[2],Vertices[3]);

	// Find all the points between lines S0 and S1, from x coordinates of P0.x to P1.x
	xMin = RoundUpwards(Vertices[0].X/GridSpacing);
	xMin = PlaceWithinGridX(xMin);
	xMax = MIN((int)(Vertices[1].X/GridSpacing),(int)(Vertices[2].X/GridSpacing));
	xMax = PlaceWithinGridX(xMax);
	if (Sides[0].yCoeff != 0) {
		for (int x = xMin; x <= xMax; x++) {
			// Set the minimum and maximum y coordinates for this vertical strip of the rectangle
			yMin = RoundUpwards(YFromX(Sides[0], x*GridSpacing)/GridSpacing);
			yMin = PlaceWithinGridY(yMin);
			yMax = (int) (YFromX(Sides[1],x*GridSpacing)/GridSpacing);
			yMax = PlaceWithinGridY(yMax);

			for (int y = yMin; y <= yMax; y++) {
				// Repeat for all z-coordinates within the height of the polygon
				for (int z = zMin; z <= zMax; z++) {
					Grid[x][y][z].Z = Impedance;
					Grid[x][y][z].PropagateFlag = PropagateFlag;
				}
			}
		}
	}

	// Find all the points between lines S2 and S1, from x coordinates of P1.x to P2.x
	xMin = MIN(RoundUpwards(Vertices[1].X/GridSpacing),RoundUpwards(Vertices[2].X/GridSpacing));
	xMin = PlaceWithinGridX(xMin);
	xMax = MAX((int)(Vertices[1].X/GridSpacing),(int)(Vertices[2].X/GridSpacing));
	xMax = PlaceWithinGridX(xMax);

	for (int x = xMin; x <= xMax; x++) {
		// Set the minimum and maximum y coordinates for this vertical strip of the rectangle
		if (Vertices[1].X <= Vertices[2].X) {
			yMin = RoundUpwards(YFromX(Sides[2], x*GridSpacing)/GridSpacing);
			yMax = (int) (YFromX(Sides[1],x*GridSpacing)/GridSpacing);
		}
		else {
			yMin = RoundUpwards(YFromX(Sides[0], x*GridSpacing)/GridSpacing);
			yMax = (int) (YFromX(Sides[3],x*GridSpacing)/GridSpacing);
		}
		yMin = PlaceWithinGridY(yMin);
		yMax = PlaceWithinGridY(yMax);
		
		for (int y = yMin; y <= yMax; y++) {
			// Repeat for all z-coordinates within the height of the polygon
			for (int z = zMin; z <= zMax; z++) {
				Grid[x][y][z].Z = Impedance;
				Grid[x][y][z].PropagateFlag = PropagateFlag;
			}
		}
	}

	// Find all the points between lines S2 and S3, from x coordinates of P2.x to P3.x
	if (Sides[3].yCoeff != 0) {
		xMin = MAX(RoundUpwards(Vertices[1].X/GridSpacing),RoundUpwards(Vertices[2].X/GridSpacing));
		xMin = PlaceWithinGridX(xMin);
		xMax = (int)(Vertices[3].X/GridSpacing);
		xMax = PlaceWithinGridX(xMax);

		for (int x = xMin; x <= xMax; x++) {
			// Set the minimum and maximum y coordinates for this vertical strip of the rectangle
			yMin = RoundUpwards(YFromX(Sides[2], x*GridSpacing)/GridSpacing);
			yMin = PlaceWithinGridY(yMin);
			yMax = (int) (YFromX(Sides[3],x*GridSpacing)/GridSpacing);
			yMax = PlaceWithinGridY(yMax);

			for (int y = yMin; y <= yMax; y++) {
				// Repeat for all z-coordinates within the height of the polygon
				for (int z = zMin; z <= zMax; z++) {
					Grid[x][y][z].Z = Impedance;
					Grid[x][y][z].PropagateFlag = PropagateFlag;
				}
			}
		}
	}
}
