// choose player pictures and map in beginning of game

#ifndef CHOOSER_H
#define CHOOSER_H

#include <SDL2/SDL.h>
#include "button.h"
#include "camera.h"
#include "ellipsoid.h"
#include "misc.h"
#include "mapeditor.h"  // IWYU pragma: keep

struct ChooserPlayerStuff {
	const struct EllipsoidPic *epic;   // pointer into a value in player_epics

	// rest of this struct isn't meant to be used outside chooser.c
	int leftx;
	float anglediff;    // how much the player chooser is about to spin
	struct Button prevbtn, nextbtn;
	struct Camera cam;
	int namew, nameh;   // size of name display text
};

struct ChooserMapStuff {
	struct Map *maps;
	int nmaps;
	int mapidx;

	// rest of this struct isn't meant to be used outside chooser.c
	struct Button prevbtn, nextbtn, editbtn, cpbtn;
};

struct Chooser {
	struct ChooserPlayerStuff playerch[2];
	struct ChooserMapStuff mapch;

	// rest of this struct isn't meant to be used outside chooser.c
	SDL_Window *win;
	SDL_Surface *winsurf;
	struct Ellipsoid ellipsoids[50];
	struct Button playbtn;
	SDL_Surface *editorsurf;
	struct MapEditor *editor;  // Not actually used for editing, only to preview
};

/*
Reuse the same Chooser for multiple calls of chooser_run() so that it remembers
the choices.
*/
void chooser_init(struct Chooser *ch, SDL_Window *win);
void chooser_destroy(const struct Chooser *ch);

enum MiscState chooser_run(struct Chooser *ch);

#endif     // CHOOSER_H
