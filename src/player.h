#ifndef PLAYER_H
#define PLAYER_H

#include "camera.h"
#include "ball.h"

struct Player {
	struct Ball *ball;
	struct Camera cam;
	float angle;
	int turning;   // +1 for left, -1 for right, 0 for nothing
	bool moving;
};

// run these once per frame
void player_turn(struct Player *plr, unsigned int fps);
void player_move(struct Player *plr, unsigned int fps);

/*
This is ran automatically by player_turn and player_move, but must be also called
to set up stuff when the game starts.
*/
void player_update(struct Player *plr);


#endif   // PLAYER_H
