// choose player pictures and place in beginning of game

#ifndef PLAYERCHOOSER_H
#define PLAYERCHOOSER_H

#include "ellipsoidpic.h"
#include "misc.h"
#include "place.h"
#include <SDL2/SDL.h>

/*
Resulting ellipsoid pics are from player_get_epics(). Returns true to play
game, false to quit.
*/
enum MiscState chooser_run(
	SDL_Window *win,
	const struct EllipsoidPic **plr1epic, const struct EllipsoidPic **plr2epic,
	const struct Place **pl);

#endif     // PLAYERCHOOSER_H
