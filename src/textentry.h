#ifndef TEXTENTRY_H
#define TEXTENTRY_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct TextEntry {
	SDL_Surface *surf;  // TODO: rename to wndsurf, must be window surface
	SDL_Rect rect;
	char *text;  // must have room for maxlen+1 chars
	int maxlen;
	int fontsz;
	bool redraw;  // true after changing text, false after textentry_show()
	char *cursor;  // pointer into text, NULL to not show

	void (*changecb)(void *data);
	void *changecbdata;

	// Do not use the rest outside textentry.c
	uint32_t lastredraw;
	uint32_t blinkstart;
	TTF_Font *font;  // FIXME: TTF_CloseFont
};

// call this FPS times per second
void textentry_show(struct TextEntry *te);

// returns whether text changed, shows as necessary
bool textentry_handle_event(struct TextEntry *te, const SDL_Event *e);

#endif  // TEXTENTRY_H
