#include "button.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include "log.h"
#include "misc.h"
#include "../stb/stb_image.h"

static SDL_Surface *load_image(const char *path)
{
	int fmt, w, h;
	unsigned char *data = stbi_load(path, &w, &h, &fmt, 4);
	if (!data)
		log_printf_abort("loading image from '%s' failed: %s", path, stbi_failure_reason());

	// SDL_CreateRGBSurfaceWithFormatFrom docs have example code for using it with stbi :D
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(
		data, w, h, 32, 4*w, SDL_PIXELFORMAT_RGBA32);
	if (!s)
		log_printf_abort("SDL_CreateRGBSurfaceWithFormatFrom failed: %s", SDL_GetError());
	assert(s->pixels == data);
	return s;
}

/*
This must be global because it's cleaned with atexit callback, and there's
no way to pass an argument into an atexit callback.
*/
static SDL_Surface *image_surfaces[BUTTON_ALLFLAGS + 1] = {0};

static void free_image_surfaces(void)
{
	for (int i = 0; i < sizeof(image_surfaces)/sizeof(image_surfaces[0]); i++) {
		if (image_surfaces[i]) {
			stbi_image_free(image_surfaces[i]->pixels);
			SDL_FreeSurface(image_surfaces[i]);
		}
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
		char path[100];
		snprintf(path, sizeof path, "buttons/%s/%s/%s",
			(f & BUTTON_BIG) ? "big" : "small",
			(f & BUTTON_VERTICAL) ? "vertical" : "horizontal",
			(f & BUTTON_DISABLED) ? "disabled.png" : (
				(f & BUTTON_PRESSED) ? "pressed.png" : "normal.png"
			)
		);
		image_surfaces[f] = load_image(path);
	}
	return image_surfaces[f];
}

#define PADDING 15
int button_width(enum ButtonFlags f)  { return get_image(f)->w + PADDING; }
int button_height(enum ButtonFlags f) { return get_image(f)->h + PADDING; }

void button_show(struct Button *butt)
{
	misc_blit_with_center(get_image(butt->flags), butt->destsurf, &butt->center);
	if (butt->imgpath) {
		SDL_Surface *s = load_image(butt->imgpath);
		misc_blit_with_center(s, butt->destsurf, &butt->center);
		SDL_FreeSurface(s);
	}

	if (butt->text) {
		SDL_Color black = { 0x00, 0x00, 0x00, 0xff };
		int fontsz = button_height(butt->flags)/2;

		const char *newln = strchr(butt->text, '\n');
		if (newln) {
			assert(strrchr(butt->text, '\n') == newln);   // no more than one '\n'

			char line1[100];
			snprintf(line1, sizeof line1, "%.*s", (int)(newln - butt->text), butt->text);
			const char *line2 = newln + 1;

			fontsz = (int)(fontsz * 0.7f);
			SDL_Surface *s1 = misc_create_text_surface(line1, black, fontsz);
			SDL_Surface *s2 = misc_create_text_surface(line2, black, fontsz);

			misc_blit_with_center(
				s1, butt->destsurf, &(SDL_Point){ butt->center.x, butt->center.y - s1->h/2 });
			misc_blit_with_center(
				s2, butt->destsurf, &(SDL_Point){ butt->center.x, butt->center.y + s2->h/2 });

			SDL_FreeSurface(s1);
			SDL_FreeSurface(s2);
		} else {
			SDL_Surface *s = misc_create_text_surface(butt->text, black, 50);
			misc_blit_with_center(s, butt->destsurf, &butt->center);
			SDL_FreeSurface(s);
		}
	}
}

static bool mouse_on_button(const SDL_MouseButtonEvent *me, const struct Button *butt)
{
	return fabsf(me->x - butt->center.x) < get_image(butt->flags)->w/2 &&
			fabsf(me->y - butt->center.y) < get_image(butt->flags)->h/2;
}

static bool scancode_matches_button(int sc, const struct Button *butt)
{
	for (int i = 0; i < sizeof(butt->scancodes)/sizeof(butt->scancodes[0]); i++) {
		if (butt->scancodes[i] != 0 && butt->scancodes[i] == sc)
			return true;
	}
	return false;
}

void button_handle_event(const SDL_Event *evt, struct Button *butt)
{
	if ((
		(evt->type == SDL_MOUSEBUTTONDOWN && mouse_on_button(&evt->button, butt)) ||
		(evt->type == SDL_KEYDOWN && scancode_matches_button(evt->key.keysym.scancode, butt))
	) && !(butt->flags & BUTTON_PRESSED)) {
		butt->flags |= BUTTON_PRESSED;
	} else if ((
		(evt->type == SDL_MOUSEBUTTONUP && mouse_on_button(&evt->button, butt)) ||
		(evt->type == SDL_KEYUP && scancode_matches_button(evt->key.keysym.scancode, butt))
	) && (butt->flags & BUTTON_PRESSED)) {
		butt->flags &= ~BUTTON_PRESSED;
		butt->onclick(butt->onclickdata);
	} else if (evt->type == SDL_MOUSEBUTTONUP && (butt->flags & BUTTON_PRESSED)) {
		// if button has been pressed and mouse has been moved away, unpress button but don't click
		butt->flags &= ~BUTTON_PRESSED;
	} else {
		// don't call button_show()
		return;
	}
	button_show(butt);
}
