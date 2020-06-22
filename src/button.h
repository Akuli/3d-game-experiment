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
#define BUTTON_SIZE_SMALL 50
#define BUTTON_SIZE_BIG 120

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

	SDL_Surface *destsurf;
	SDL_Point center;

	// these are set in button_show()
	int width, height;

	void (*onclick)(void *onclickdata);
	void *onclickdata;
};

/*
Call this to show a button after creating a new button, blanking the screen or
changing anything that affects how the button looks.

Don't call this in a loop that runs FPS times per second. Blitting in SDL is
surprisingly slow, and this function needs to blit stuff.
*/
void button_show(struct Button *butt);

// does nothing for events not related to the button
void button_handle_event(const SDL_Event *evt, struct Button *butt);


#endif    // BUTTON_H
