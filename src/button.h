#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <SDL2/SDL.h>

struct Button {
	// imgpath image and text are drawn on top of a generic button background image
	const char *imgpath;
	const char *text;

	enum ButtonFlags {
		BUTTON_BIG = 0x01,
		BUTTON_HORIZONTAL = 0x02,
		BUTTON_PRESSED = 0x04,
	} flags;

	/*
	Which keyboard key presses correspond to this button? Zeroes in this list are
	ignored, so this works:

		struct Button b = {
			...
			.scancodes = { SDL_SCANCODE_BLA },
			...
		};

	This sets the first scancode to SDL_SCANCODE_BLA and rest to zero.
	*/
	int scancodes[5];

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

// Calculate width and height with BUTTON_BIG and BUTTON_HORIZONTAL flags
int button_width(enum ButtonFlags f);
int button_height(enum ButtonFlags f);


#endif    // BUTTON_H
