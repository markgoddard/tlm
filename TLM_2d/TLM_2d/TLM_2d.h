#ifndef TLM_2D_H
#define TLM_2D_H

// Structure to hold a single node
typedef struct {	// Current state
					double V;
					// Input variables 
					double Vi[4];
					// Output variables
					double Vo[4];
					// Peak voltage observed
					double Vmax;
					// Node properties
				} Node;


#define SQUARE(a) ((a)*(a))
#endif 