#ifndef MAPEDITOR_H
#define MAPEDITOR_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "place.h"
#include "misc.h"

// Needs array of all places because it's possible to delete a place
enum MiscState mapeditor_run(
	SDL_Window *wnd,
	struct Place *places, int *nplaces, int placeidx,
	const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic);

#endif   // MAPEDITOR_H
