#include "button.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "misc.h"
#include "log.h"

/*
This must be global because it's cleaned with atexit callback, and there's
no way to pass an argument into an atexit callback.
*/
static SDL_Surface *image_surfaces[BUTTON_ALLFLAGS + 1] = {0};

static void free_image_surfaces(void)
{
	for (int i = 0; i < sizeof(image_surfaces)/sizeof(image_surfaces[0]); i++) {
		if (image_surfaces[i])
			free_image_surface(image_surfaces[i]);
	}
}

static SDL_Surface *get_image(enum ButtonFlags f)
{
	static bool atexitdone = false;
	if (!atexitdone) {
		atexit(free_image_surfaces);
		atexitdone = true;
	}

	if (!image_surfaces[f]) {
		char path[100] = "assets/buttons/";

		switch((int)f & (BUTTON_TINY | BUTTON_SMALL | BUTTON_BIG | BUTTON_THICK)) {
			case BUTTON_TINY: strcat(path, "tiny/"); break;
			case BUTTON_SMALL: strcat(path, "small/"); break;
			case BUTTON_BIG: strcat(path, "big/"); break;
			case BUTTON_THICK: strcat(path, "thick/"); break;
			case 0: strcat(path, "medium/"); break;
			default: log_printf_abort("bad button size flags: %#x", f);
		}

		if (f & BUTTON_VERTICAL)
			strcat(path, "vertical/");
		else
			strcat(path, "horizontal/");

		// If it is pressed and disabled, just consider it disabled
		if (f & BUTTON_DISABLED)
			strcat(path, "disabled.png");
		else if (f & BUTTON_PRESSED)
			strcat(path, "pressed.png");
		else
			strcat(path, "normal.png");

		image_surfaces[f] = create_image_surface(path);
	}
	return image_surfaces[f];
}

static int get_margin(enum ButtonFlags f) {
	if (f & BUTTON_TINY)
		return 2;
	return 8;
}

int button_width(enum ButtonFlags f)  { return get_image(f)->w + 2*get_margin(f); }
int button_height(enum ButtonFlags f) { return get_image(f)->h + 2*get_margin(f); }

void button_show(const struct Button *butt)
{
	blit_with_center(get_image(butt->flags), butt->destsurf, &butt->center);
	SDL_assert(!(butt->imgpath && butt->text));

	if (butt->imgpath) {
		SDL_Surface *s = create_image_surface(butt->imgpath);
		blit_with_center(s, butt->destsurf, &butt->center);
		free_image_surface(s);
	}

	if (butt->text) {
		SDL_Color black = { 0x00, 0x00, 0x00, 0xff };
		int fontsz;
		if (butt->flags & BUTTON_TINY)
			fontsz = 16;
		else if (butt->flags & BUTTON_THICK)
			fontsz = 40;
		else
			fontsz = button_height(butt->flags)/2;

		const char *newln = strchr(butt->text, '\n');
		if (newln) {
			SDL_assert(strrchr(butt->text, '\n') == newln);   // no more than one '\n'

			char line1[100];
			snprintf(line1, sizeof line1, "%.*s", (int)(newln - butt->text), butt->text);
			const char *line2 = newln + 1;

			fontsz = (int)(fontsz * 0.65f);
			SDL_Surface *s1 = create_text_surface(line1, black, fontsz);
			SDL_Surface *s2 = create_text_surface(line2, black, fontsz);

			blit_with_center(
				s1, butt->destsurf, &(SDL_Point){ butt->center.x, butt->center.y - s1->h/2 });
			blit_with_center(
				s2, butt->destsurf, &(SDL_Point){ butt->center.y, butt->center.y + s2->h/2 });

			SDL_FreeSurface(s1);
			SDL_FreeSurface(s2);
		} else {
			SDL_Surface *s = create_text_surface(butt->text, black, fontsz);
			blit_with_center(s, butt->destsurf, &butt->center);
			SDL_FreeSurface(s);
		}
	}
}

static bool mouse_on_button(const SDL_MouseButtonEvent *me, const struct Button *butt)
{
	return abs(me->x - butt->center.x) < get_image(butt->flags)->w/2 &&
			abs(me->y - butt->center.y) < get_image(butt->flags)->h/2;
}

static bool scancode_matches_button(const SDL_Event *evt, const struct Button *butt)
{
	int sc = normalize_scancode(evt->key.keysym.scancode);
	for (int i = 0; i < sizeof(butt->scancodes)/sizeof(butt->scancodes[0]); i++) {
		if (butt->scancodes[i] != 0 && butt->scancodes[i] == sc)
			return true;
	}
	return false;
}

void button_handle_event(const SDL_Event *evt, struct Button *butt)
{
	if (butt->flags & BUTTON_DISABLED)
		return;

	bool click = false;
	if ((
		(evt->type == SDL_MOUSEBUTTONDOWN && mouse_on_button(&evt->button, butt)) ||
		(evt->type == SDL_KEYDOWN && scancode_matches_button(evt, butt))
	) && !(butt->flags & BUTTON_PRESSED)) {
		butt->flags |= BUTTON_PRESSED;
		if (butt->flags & BUTTON_STAYPRESSED)
			click = true;
	} else if ((
		(evt->type == SDL_MOUSEBUTTONUP && mouse_on_button(&evt->button, butt)) ||
		(evt->type == SDL_KEYUP && scancode_matches_button(evt, butt))
	) && (butt->flags & BUTTON_PRESSED) && !(butt->flags & BUTTON_STAYPRESSED)) {
		butt->flags &= ~BUTTON_PRESSED;
		click = true;
	} else if (evt->type == SDL_MOUSEBUTTONUP && (butt->flags & BUTTON_PRESSED) && !(butt->flags & BUTTON_STAYPRESSED)) {
		// if button has been pressed and mouse has been moved away, unpress button but don't click
		butt->flags &= ~BUTTON_PRESSED;
	} else {
		// nothing has changed, no need to show button
		return;
	}

	button_show(butt);
	if (click) {
		log_printf("clicking button \"%s\"", butt->text);
		butt->onclick(butt->onclickdata);  // may free the button, must be last
	}
}
