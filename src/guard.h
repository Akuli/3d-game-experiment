#ifndef GUARD_H
#define GUARD_H

#include "ellipsoid.h"
#include "place.h"
#include "player.h"
#include <SDL2/SDL.h>

#define GUARD_XZRADIUS 0.25f

// call this before any other guard functions
void guard_init_epic(const SDL_PixelFormat *fmt);

/*
All guards added to exactly the same x and z values go on top of each other, so
the y coordinate of the center is not always used exactly as it is given.
The center argument is the center of the bottom of the visible half of the guard.
Guards array never grows larger than MAX_UNPICKED_GUARDS.
The _random suffixed function chooses the center randomly to fit the place.
These functions return the number of guards actually added (without overflowing the array)
*/
int guard_create_unpickeds_center(
	struct Ellipsoid *guards, int *nguards, int howmany2add, Vec3 center);
int guard_create_unpickeds_random(
	struct Ellipsoid *guards, int *nguards, int howmany2add, const struct Place *pl);

// don't run this for picked guards
void guard_unpicked_eachframe(struct Ellipsoid *el);

/*
example:

	struct Ellipsoid arr[MAX_PICKED_GUARDS_TO_DISPLAY_PER_PLAYER];
	int arrlen = guard_create_picked(arr, plr);
	for (int i = 0; i < n; i++)
		draw_ellipsoid_on_screen(arr[i]);

this is meant to be called on each frame for drawing
*/
int guard_create_picked(struct Ellipsoid *arr, const struct Player *plr);


#endif    // GUARD_H
