#ifndef PLAYER_H
#define PLAYER_H

#include "camera.h"
#include "ellipsoid.h"
#include "place.h"
#include "wall.h"
#include "../generated/filelist.h"
#include <SDL2/SDL.h>

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
	int jumpframe;   // how many frames since jump started, 0 for not jumping

	// negative after game over
	int nguards;
};

// Call player_init_epics() before accessing player_ellipsoidpics
extern struct EllipsoidPic player_ellipsoidpics[FILELIST_NPLAYERS];
void player_init_epics(const SDL_PixelFormat *fmt);

// Get name of player from a pointer to an ellipsoid pic from player_get_epics()
void player_epic_name(const struct EllipsoidPic *epic, char *name, int sizeofname);

// run before showing stuff to user
void player_eachframe(struct Player *plr, const struct Place *pl);

// key press callbacks. dir values: -1 for left, +1 for right
void player_set_turning(struct Player *plr, int dir, bool turn);
void player_set_moving(struct Player *plr, bool mv);
void player_set_flat(struct Player *plr, bool flat);


#endif   // PLAYER_H
