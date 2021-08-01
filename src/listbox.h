#ifndef LISTBOX_H
#define LISTBOX_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "button.h"

// must match image size, see scripts/resize_images
#define LISTBOX_WIDTH 500

struct ListboxEntry {
	const char *text;
	struct Button buttons[3];
};

struct Listbox {
	SDL_Surface *destsurf;  // must be surface of whole window, for button click events
	SDL_Rect destrect;

	int selectidx;  // something always selected, no -1 or whatever
	bool redraw;   // set to true after entries change, needed because redrawing is slow

	// very similar to buttons
	int upscancodes[2];
	int downscancodes[2];

	// return value can be e.g. statically allocated
	// must return NULL for index out of range
	const struct ListboxEntry *(*getentry)(void *getentrydata, int idx);
	void *getentrydata;

	// buttons have state, can't just be in on-the-fly generated entries
	struct Button *visiblebuttons;
	int nvisiblebuttons;

	// don't use rest of this outside listbox.c
	SDL_Surface *bgimg;
	SDL_Surface *selectimg;
	int firstvisible;  // scrolling
};

// fill lb->destsurf, lb->entries etc before initing
void listbox_init(struct Listbox *lb);
void listbox_destroy(const struct Listbox *lb);

// sets lb->redraw to false, does nothing if already false
void listbox_show(struct Listbox *lb);

// Return text of clicked button, or NULL if nothing clicked
void listbox_handle_event(struct Listbox *lb, const SDL_Event *e);

#endif   // LISTBOX_H
