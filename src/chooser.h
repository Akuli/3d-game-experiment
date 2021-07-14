// choose player pictures and place in beginning of game

#ifndef CHOOSER_H
#define CHOOSER_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "button.h"
#include "camera.h"
#include "ellipsoid.h"
#include "misc.h"

struct ChooserPlayerStuff {
	const struct EllipsoidPic *epic;   // pointer into player_getepics()

	// rest of this struct isn't meant to be used outside chooser.c
	int leftx;
	float anglediff;    // how much the player chooser is about to spin
	struct Button prevbtn, nextbtn;
	struct Camera cam;
	int namew, nameh;   // size of name display text
};

struct ChooserPlaceStuff {
	struct Place *pl;   // pointer into place_list()

	// rest of this struct isn't meant to be used outside chooser.c
	struct Button prevbtn, nextbtn;
	struct Camera cam;
};

struct Chooser {
	struct ChooserPlayerStuff playerch[2];
	struct ChooserPlaceStuff placech;

	// is "practice without enemies" easter egg selected?
	bool withoutenemies;

	// rest of this struct isn't meant to be used outside chooser.c
	SDL_Window *win;
	SDL_Surface *winsurf;
	struct Ellipsoid ellipsoids[50];
	struct Button bigplaybtn, editbtn;
	SDL_Surface *withoutenemiestxt;
	SDL_Rect withoutenemiesrect;
};

/*
Reuse the same Chooser for multiple calls of chooser_run() so that it remembers
the choices.
*/
void chooser_init(struct Chooser *ch, SDL_Window *win);
void chooser_destroy(const struct Chooser *ch);

enum MiscState chooser_run(struct Chooser *ch);

#endif     // CHOOSER_H
