/*********************************************************************************************/
//
//	Project:	Event Based Scalar TLM Model for Urban Environments
//
//	Author:		Mark Goddard
//	Date:		27/10/2008
//	File:		TLMScene.h
//
/*********************************************************************************************/

#ifndef TLM_SCENE_H
#define TLM_SCENE_H

// Function prototypes
bool ReadSceneFile(void);
int PlaceWithinGridX(int x);
int PlaceWithinGridY(int y);
int PlaceWithinGridZ(int z);

#endif //TLM_SCENE_H