// The event loop that runs when actually playing the game

#ifndef PLAY_H
#define PLAY_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "misc.h"
#include "map.h"

// sets winnerpic when returns MISC_STATE_GAMEOVER
enum MiscState play_the_game(
	SDL_Window *wnd,
	const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic,
	const struct EllipsoidPic **winnerpic,
	const struct Map *map);


#endif   // PLAY_H
