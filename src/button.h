#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <SDL2/SDL.h>

/*
These button sizes is along the shorter side:

	                       _
	,---------------------. |
	|  Horizontal button  | |size
	`---------------------'_|


	 ,----------.
	 |          |
	 |          |
	 | Vertical |
	 |  button  |
	 |          |
	 |          |
	 `----------'
	|____________|
	     size

The buttons are actually slightly smaller than these sizes, so you should get
nice padding if you use these sizes.
*/
#define SMALL_BUTTON_SIZE 50
#define BIG_BUTTON_SIZE 350

struct Button {
	// imgpath image and text are drawn on top of a generic button background image
	const char *imgpath;
	const char *text;

	bool big;
	bool horizontal;
	bool pressed;

	/*
	Which keyboard key press corresponds to this button? Set this to -1 if you
	don't want to associate a key with this button. That works because SDL doesn't
	use any negative scancodes (see SDL_scancode.h).
	*/
	int scancode;

	// cachesurf is blitted to destsurf on each frame
	SDL_Surface *cachesurf;
	SDL_Surface *destsurf;
	SDL_Point center;

	void (*onclick)(void *onclickdata);
	void *onclickdata;
};

// call this after changing anything that affects how the button looks
void button_refresh(struct Button *butt);

// draw button to its destsurf
void button_show(const struct Button *butt);

// does nothing for events not related to the button
void button_handle_event(const SDL_Event *evt, struct Button *butt);


#endif    // BUTTON_H
