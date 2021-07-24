#ifndef LISTBOX_H
#define LISTBOX_H

#include <SDL2/SDL.h>
#include <stdbool.h>

struct ListboxEntry {
	const char *text;
	bool hasbuttons;
};

struct Listbox {
	SDL_Surface *destsurf;
	struct ListboxEntry *entries;
	int nentries;
	int selectidx;  // something always selected, no -1 or whatever
	const char *buttontexts[2];  // used only with hasbuttons=true entries
	bool redraw;   // set to true after changing anything

	// very similar to buttons
	int upscancodes[2];
	int downscancodes[2];

	// don't use rest of this outside listbox.c
	SDL_Surface *bgimg;
	SDL_Surface *selectimg;
};

// fill destsurf, entries etc before initing
void listbox_init(struct Listbox *lb);
void listbox_destroy(const struct Listbox *lb);

void listbox_show(struct Listbox *lb);
void listbox_handle_event(struct Listbox *lb, const SDL_Event *e);

#endif   // LISTBOX_H
