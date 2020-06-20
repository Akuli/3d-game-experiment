// The event loop that runs when actually playing the game

#ifndef GAME_H
#define GAME_H

#include "place.h"
#include <SDL2/SDL.h>

// Returns winner ellipsoid pic or NULL to exit game
const struct EllipsoidPic *game_run(
	SDL_Window *win,
	const struct EllipsoidPic *plr1pic, const struct EllipsoidPic *plr2pic,
	const struct Place *pl);


#endif   // GAME_H
