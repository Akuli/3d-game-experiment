#ifndef PLAYER_H
#define PLAYER_H

#include "camera.h"
#include "ellipsoid.h"
#include "wall.h"

// smallest possible height of the player (then ellipsoid.yradius is half of this)
#define PLAYER_HEIGHT_FLAT 0.1f

struct Player {
	struct EllipsoidPic epic;
	struct Ellipsoid ellipsoid;
	struct Camera cam;
	int turning;   // see player_set_turning()
	bool moving;
	bool flat;
	unsigned int jumpframe;   // how many frames since jump started, 0 for not jumping
};

// run before showing stuff to user
void player_eachframe(struct Player *plr, unsigned int fps, const struct Wall *walls, size_t nwalls);

// key press callbacks. dir values: -1 for left, +1 for right
void player_set_turning(struct Player *plr, int dir, bool turn);
void player_set_moving(struct Player *plr, bool mv);
void player_set_flat(struct Player *plr, bool flat);


#endif   // PLAYER_H
