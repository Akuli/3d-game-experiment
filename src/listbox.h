#ifndef LISTBOX_H
#define LISTBOX_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "button.h"

// must match image size, see scripts/resize_images
#define LISTBOX_WIDTH 500
#define LISTBOX_BUTTONS_MAX 3

struct ListboxEntry {
	const char *text;
	const char *buttontexts[LISTBOX_BUTTONS_MAX];

	// don't use rest of this outside listbox.c
	struct Button buttons[LISTBOX_BUTTONS_MAX];
};

struct Listbox {
	SDL_Surface *destsurf;  // must be surface of whole window, for button click events
	SDL_Rect destrect;

	struct ListboxEntry *entries;
	int nentries;
	int selectidx;  // something always selected, no -1 or whatever
	bool redraw;   // set to true after changing anything

	// very similar to buttons
	int upscancodes[2];
	int downscancodes[2];

	// don't use rest of this outside listbox.c
	SDL_Surface *bgimg;
	SDL_Surface *selectimg;
	const char *clicktext;
};

// fill lb->destsurf, lb->entries etc before initing
void listbox_init(struct Listbox *lb);
void listbox_destroy(const struct Listbox *lb);

// sets lb->redraw to false
void listbox_show(struct Listbox *lb);

// Return text of clicked button, or NULL if nothing clicked
const char *listbox_handle_event(struct Listbox *lb, const SDL_Event *e);

#endif   // LISTBOX_H
