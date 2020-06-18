#ifndef PLAYER_H
#define PLAYER_H

#include "camera.h"
#include "ellipsoid.h"
#include "wall.h"

// smallest possible height of the player (then ellipsoid.yradius is half of this)
#define PLAYER_HEIGHT_FLAT 0.1f

/*
xzradius must not be >0.25, because two players must fit between walls
that are distance 1 apart from each other. If they don't fit, then the
players will end up sticking out to the other side of the walls, which
causes weird behaviour.
*/
#define PLAYER_XRADIUS 0.25f

// isn't correct when player is flat
#define PLAYER_YRADIUS_NOFLAT 0.45f

struct Player {
	struct EllipsoidPic epic;
	struct Ellipsoid ellipsoid;
	struct Camera cam;

	int turning;   // see player_set_turning()
	bool moving;
	bool flat;
	int jumpframe;   // how many frames since jump started, 0 for not jumping

	// negative after game over
	int nguards;
};

// run before showing stuff to user
void player_eachframe(struct Player *plr, const struct Wall *walls, int nwalls);

// key press callbacks. dir values: -1 for left, +1 for right
void player_set_turning(struct Player *plr, int dir, bool turn);
void player_set_moving(struct Player *plr, bool mv);
void player_set_flat(struct Player *plr, bool flat);


#endif   // PLAYER_H
