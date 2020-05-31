#ifndef PLAYER_H
#define PLAYER_H

#include "camera.h"
#include "ball.h"
#include "wall.h"

/*
The radius of the player in the horizontal xz plane is always this, even when
the player is flat or jumping
*/
#define PLAYER_RADIUS_XZ 0.3f

// smallest possible height of the player
#define PLAYER_HEIGHT_FLAT 0.1f

struct Player {
	struct Ball *ball;
	struct Camera cam;
	float angle;
	int turning;   // see player_set_turning()
	bool moving;
	bool flat;
	unsigned int jumpframe;   // how many frames since jump started, 0 for not jumping
};

// run before showing stuff to user
void player_eachframe(struct Player *plr, unsigned int fps, const struct Wall *walls, size_t nwalls);

// key press callbacks. dir values: +1 for left, -1 for right
void player_set_turning(struct Player *plr, int dir, bool turn);
void player_set_moving(struct Player *plr, bool mv);
void player_set_flat(struct Player *plr, bool flat);


#endif   // PLAYER_H
