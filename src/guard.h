#ifndef GUARD_H
#define GUARD_H

#include "ellipsoid.h"
#include "place.h"
#include "player.h"
#include <SDL2/SDL.h>

// call this before any other guard functions
void guard_init_epic(const SDL_PixelFormat *fmt);

// all added guards go on top of each other, guards array never grows larger than MAX_UNPICKED_GUARDS
void guard_create_unpickeds(
	const struct Place *pl, struct Ellipsoid *guards, int *nguards, int howmany2add);

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
