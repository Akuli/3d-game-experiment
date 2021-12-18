#ifndef GAMEOVER_H
#define GAMEOVER_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "misc.h"

enum State game_over(struct SDL_Window *wnd, const struct EllipsoidPic *winnerpic);


#endif    // GAMEOVER_H
