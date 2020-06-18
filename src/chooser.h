// choose player pictures and place in beginning of game

#ifndef PLAYERCHOOSER_H
#define PLAYERCHOOSER_H

#include "ellipsoidpic.h"
#include "place.h"
#include <SDL2/SDL.h>

// Return values: true means play game, false means exit
bool chooser_run(
	SDL_Window *win,
	const struct EllipsoidPic **plr1pic, const struct EllipsoidPic **plr2pic,
	const struct Place **pl);

#endif     // PLAYERCHOOSER_H
