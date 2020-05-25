#ifndef PLAYER_H
#define PLAYER_H

#include "camera.h"
#include "sphere.h"

struct Player {
	struct Sphere *sphere;
	struct Camera cam;
	int turning;   // +1 for left, -1 for right, 0 for nothing
	bool moving;
};

// run these once per frame
void player_turn(struct Player *plr, unsigned int fps);
void player_move(struct Player *plr, unsigned int fps);

/*
This is ran automatically by player_turn and player_move, but must be also called
to set up some camera stuff when the game starts.
*/
void player_updatecam(struct Player *plr);


#endif   // PLAYER_H
