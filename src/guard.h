#ifndef GUARD_H
#define GUARD_H

#include "ellipsoid.h"
#include "place.h"
#include "player.h"
#include <SDL2/SDL.h>

// max number of guards
#define GUARD_MAX 128

// resulting guard has random location and hasn't been picked up by any player or destroyed by an enemy
struct Ellipsoid guard_create_nonpicked(const struct Place *pl, const SDL_PixelFormat *fmt);

// don't run this for non-picked guards
void guard_nonpicked_eachframe(struct Ellipsoid *el);

/*
example:

	struct Ellipsoid arr[GUARD_MAX];
	int arrlen = guard_create_picked(arr, plr);
	for (int i = 0; i < n; i++)
		draw_ellipsoid_on_screen(arr[i]);

this is meant to be called on each frame for drawing
*/
int guard_create_picked(struct Ellipsoid *arr, const struct Player *plr);


#endif    // GUARD_H
