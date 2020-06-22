// choose player pictures and place in beginning of game

#ifndef CHOOSER_H
#define CHOOSER_H

#include "../generated/filelist.h"
#include "button.h"
#include "ellipsoidpic.h"
#include "misc.h"
#include "place.h"
#include <SDL2/SDL.h>

struct ChooserPlayerStuff {
	const struct EllipsoidPic *epic;   // pointer into player_getepics()

	// rest of this struct isn't meant to be used outside chooser.c
	int leftx;
	float anglediff;    // how much the player chooser is about to spin
	struct Button prevbtn, nextbtn;
	struct Camera cam;
	SDL_Surface *nametextsurf;
};

struct Chooser {
	struct ChooserPlayerStuff playerch[2];

	// rest of this struct isn't meant to be used outside chooser.c
	SDL_Window *win;
	SDL_Surface *winsurf;
	struct Ellipsoid ellipsoids[FILELIST_NPLAYERS];
	struct Button bigplaybtn;
};

/*
Reuse the same Chooser for multiple calls of chooser_run() so that it remembers
the choices.
*/
void chooser_init(struct Chooser *ch, SDL_Window *win);
void chooser_destroy(const struct Chooser *ch);

enum MiscState chooser_run(struct Chooser *ch);

#endif     // CHOOSER_H
