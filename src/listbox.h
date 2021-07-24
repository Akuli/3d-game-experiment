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

	// don't use rest of this outside listbox.c
	SDL_Surface *bgimg;
	SDL_Surface *selectimg;
};

// fill destsurf, entries etc before initing
void listbox_init(struct Listbox *lb);
void listbox_destroy(const struct Listbox *lb);

void listbox_show(const struct Listbox *lb);

#endif   // LISTBOX_H
