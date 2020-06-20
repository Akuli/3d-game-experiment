#ifndef GAMEOVER_H
#define GAMEOVER_H

#include <SDL2/SDL.h>
#include "ellipsoidpic.h"
#include "misc.h"

enum MiscState game_over(
	struct SDL_Window *wnd, const struct EllipsoidPic *winnerpic);


#endif    // GAMEOVER_H
