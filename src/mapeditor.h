#ifndef MAPEDITOR_H
#define MAPEDITOR_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "map.h"
#include "misc.h"

// Needs array of all maps because it's possible to delete a map
enum MiscState mapeditor_run(
	SDL_Window *wnd,
	struct Map *maps, int *nmaps, int mapidx,
	const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic);

#endif   // MAPEDITOR_H
