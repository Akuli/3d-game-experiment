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
	bool flat;
	unsigned int jumpframe;   // how many frames since jump started, 0 for not jumping
};

// run before showing stuff to user
void player_eachframe(struct Player *plr, unsigned int fps);

// key press callbacks. dir values: +1 for left, -1 for right
void player_set_turning(struct Player *plr, int dir, bool turn);
void player_set_moving(struct Player *plr, bool mv);
void player_set_flat(struct Player *plr, bool flat);


#endif   // PLAYER_H
