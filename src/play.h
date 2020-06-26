// The event loop that runs when actually playing the game

#ifndef PLAY_H
#define PLAY_H

#include "misc.h"
#include "place.h"
#include <SDL2/SDL.h>

// sets winnerpic when returns MISC_STATE_GAMEOVER
enum MiscState play_the_game(
	SDL_Window *wnd,
	const struct EllipsoidPic *plr1pic, const struct EllipsoidPic *plr2pic,
	const struct EllipsoidPic **winnerpic,
	const struct Place *pl, bool enemies);


#endif   // PLAY_H
