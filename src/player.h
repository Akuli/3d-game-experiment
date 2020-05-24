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


#endif   // PLAYER_H
