#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "ellipsoid.h"
#include "map.h"

// smallest possible height of the player (then ellipsoid.yradius is half of this)
#define PLAYER_HEIGHT_FLAT 0.1f

/*
If xzradius is just a little bit more than 0.25, then two players can be squeezed
between walls that are distance 1 apart from each other. They end up going
partially through the walls. That can happen so much that they hit enemies through
walls. If you set xzradius to >0.25, then check that this doesn't happen.
*/
#define PLAYER_XZRADIUS 0.4f

// isn't correct when player is flat
#define PLAYER_YRADIUS_NOFLAT 0.7f

struct Player {
	struct Ellipsoid ellipsoid;
	struct Camera cam;

	int turning;   // see player_set_turning()
	bool moving;
	bool flat;
	Vec3 speed;

	// negative after game over
	int nguards;
};

extern struct EllipsoidPic *const *player_epics;  // NULL terminated
extern int player_nepics;
void player_init_epics(const SDL_PixelFormat *fmt);

// run before showing stuff to user
void player_eachframe(struct Player *plr, const struct Map *map);

// key press callbacks. dir values: -1 for left, +1 for right
void player_set_turning(struct Player *plr, int dir, bool turn);
void player_set_moving(struct Player *plr, bool mv);
void player_set_flat(struct Player *plr, bool flat);

/*
If the player has picked up guards and is moving, leave one behind the players
so that others can get it.

The array never becomes longer than MAX_UNPICKED_GUARDS.
*/
void player_drop_guard(struct Player *plr, struct Ellipsoid *arr, int *arrlen);


#endif   // PLAYER_H
