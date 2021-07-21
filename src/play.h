// The event loop that runs when actually playing the game

#ifndef PLAY_H
#define PLAY_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "misc.h"
#include "place.h"

// sets winnerpic when returns MISC_STATE_GAMEOVER
enum MiscState play_the_game(
	SDL_Window *wnd,
	const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic,
	const struct EllipsoidPic **winnerpic,
	const struct Place *pl);


#endif   // PLAY_H
