// TLM_2d.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "TLM_2d.h"



void Scatter(double M[4][4], double Vi[4], double Vo[4]) {
	for (int i=0; i<4; i++) {
		Vo[i] = 0;
		for (int k=0; k<4; k++) {
			Vo[i] += Vi[k]*M[i][k];
		}
	}
}



int main(int argc, char* argv[])
{
	Node **Grid;
	int xSize = 51;
	int ySize = 51;
	const double SideScatter = 1.0/2.0/sqrt(2.0);
	const double ForwardScatter = 0.501;
	const double SingleScatter[4] = {ForwardScatter-2*SideScatter, SideScatter, ForwardScatter, SideScatter};
	//const double SingleScatter[4] = {-1.0/sqrt(2.0),0.5,1.0/sqrt(2.0),0.5};
	double ScatterMatrix[4][4];

	// Setup the scatter matrix
	for (int i = 0; i < 4; i++) {
		for (int k = 0; k < 4; k++) {
			ScatterMatrix[i][k] = SingleScatter[(k-i+4)%4];
		}
	}

	// Allocate grid memory
	Grid = (Node**)malloc(xSize*sizeof(Node*));
	for (int x = 0; x< xSize; x++) {
		Grid[x] = (Node*)malloc(ySize*sizeof(Node));
	}
	for (int x = 0; x < xSize; x++) {
		for (int y = 0; y < ySize; y++) {
			for (int i=0; i<4; i++) {
				Grid[x][y].Vo[i] = 0;
				Grid[x][y].Vi[i] = 0;
			}
			Grid[x][y].V = 0;
			Grid[x][y].Vmax = 0;
		}
	}

	// Impulse source
	for (int i=0; i<4; i++) {
		Grid[xSize/2][ySize/2].Vi[i] = 0.25;
	}

	for (int i=0; i<2000; i++) {

		// Scatter
		for (int x=0; x<xSize; x++) {
			for (int y=0; y<ySize; y++) {
				if (x==25 && y==25) {
					x=y;
				}
				Scatter(ScatterMatrix,Grid[x][y].Vi, Grid[x][y].Vo);
			}
		}

		// Connect
		for (int x=0; x<xSize; x++) {
			for (int y=0; y<ySize; y++) {
				if (x>0) {
					Grid[x][y].Vi[3] = Grid[x-1][y].Vo[1];
				}
				else {
					Grid[x][y].Vi[3] = 0;
				}
				if (x<(xSize-1)) {
					Grid[x][y].Vi[1] = Grid[x+1][y].Vo[3];
				}
				else {
					Grid[x][y].Vi[1] = 0;
				}
				if (y>0) {
					Grid[x][y].Vi[2] = Grid[x][y-1].Vo[0];
				}
				else {
					Grid[x][y].Vi[2] = 0;
				}
				if (y<(ySize-1)) {
					Grid[x][y].Vi[0] = Grid[x][y+1].Vo[2];
				}
				else {
					Grid[x][y].Vi[0] = 0;
				}
				Grid[x][y].V = Grid[x][y].Vi[0] + Grid[x][y].Vi[1] + Grid[x][y].Vi[2] + Grid[x][y].Vi[3];
				if (Grid[x][y].V != 0) {
					x=x;
				}
				if (abs(Grid[x][y].V) > Grid[x][y].Vmax) {
					Grid[x][y].Vmax = abs(Grid[x][y].V);
				}
			}
		}
	}

	FILE *File;

	fopen_s(&File, "../Final.txt", "w");

	for (int x = 0; x<xSize; x++) {
		for (int y = 0; y<ySize; y++) {
			fprintf(File, "%f\t", Grid[x][y].Vmax);
		}
		fprintf(File,"\n");
	}

	fprintf(File,"\nKappa\n");
	for (int x = 0; x<xSize; x++) {
		for (int y = 0; y<ySize; y++) {
			fprintf(File, "%f\t", 1.0/Grid[x][y].Vmax/sqrt((double)SQUARE(x-xSize/2)+(double)SQUARE(y-ySize/2)));
		}
		fprintf(File,"\n");
	}
	fclose (File);
	return 0;
}




	/*	if (i<10) {
			FILE *File;

			fopen_s(&File, "../Final.txt", "a");

			fprintf(File,"\nIteration %d\n",i);
			for (int x = 0; x<xSize; x++) {
				for (int y = 0; y<ySize; y++) {
					fprintf(File, "%f\t", Grid[x][y].Vmax);
				}
				fprintf(File,"\n");
			}
			fclose (File);
		}*/