#ifndef DELETEMAP_H
#define DELETEMAP_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "map.h"
#include "misc.h"

// returns STATE_CHOOSER or STATE_QUIT
enum State deletemap_dialog(
	struct SDL_Window *wnd, struct Map *maps, int *nmaps, int mapidx,
	const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic);

#endif    // DELETEMAP_H
