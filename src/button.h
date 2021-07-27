#ifndef BUTTON_H
#define BUTTON_H

#include <SDL2/SDL.h>

enum ButtonFlags {
	BUTTON_TINY = 0x01,
	BUTTON_SMALL = 0x02,
	BUTTON_BIG = 0x04,
	BUTTON_VERTICAL = 0x08,
	BUTTON_PRESSED = 0x10,
	BUTTON_DISABLED = 0x20,
};
#define BUTTON_ALLFLAGS (BUTTON_TINY | BUTTON_SMALL | BUTTON_BIG | BUTTON_VERTICAL | BUTTON_PRESSED | BUTTON_DISABLED)

struct Button {
	// imgpath image and text are drawn on top of a generic button background image
	const char *imgpath;
	const char *text;

	enum ButtonFlags flags;

	/*
	Which keyboard key presses correspond to this button? Zeroes in this list are
	ignored, so this works:

		struct Button b = {
			...
			.scancodes = { SDL_SCANCODE_BLA },
			...
		};

	This sets the first scancode to SDL_SCANCODE_BLA and rest to zero.

	Key bindings are listed in README. Try to keep that up to date.
	*/
	int scancodes[5];

	SDL_Surface *destsurf;
	SDL_Point center;

	void (*onclick)(void *onclickdata);   // never called when BUTTON_DISABLED is set
	void *onclickdata;
};

/*
Call this to show a button after creating a new button, blanking the screen or
changing anything that affects how the button looks.

Don't call this in a loop that runs FPS times per second. Blitting in SDL is
surprisingly slow, and this function needs to blit stuff.
*/
void button_show(const struct Button *butt);

// does nothing for events not related to the button
// calls button_show() as needed
void button_handle_event(const SDL_Event *evt, struct Button *butt);

// Calculate width and height based on BUTTON_BIG, BUTTON_SMALL, BUTTON_VERTICAL
int button_width(enum ButtonFlags f);
int button_height(enum ButtonFlags f);


#endif    // BUTTON_H
