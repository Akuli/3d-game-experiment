#include "button.h"
#include <assert.h>
#include "log.h"
#include "misc.h"
#include "../stb/stb_image.h"

static const SDL_Color black_color = { 0x00, 0x00, 0x00, 0xff };

static void add_image(const char *path, SDL_Surface *dest, SDL_Point center, int *wptr, int *hptr)
{
	int fmt, w, h;
	unsigned char *data = stbi_load(path, &w, &h, &fmt, 4);
	if (!data)
		log_printf_abort("loading image from '%s' failed: %s", path, stbi_failure_reason());

	if (wptr)
		*wptr = w;
	if (hptr)
		*hptr = h;

	// SDL_CreateRGBSurfaceWithFormatFrom docs have example code for using it with stbi :D
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(
		data, w, h, 32, 4*w, SDL_PIXELFORMAT_RGBA32);
	if (!s)
		log_printf_abort("SDL_CreateRGBSurfaceWithFormatFrom failed: %s", SDL_GetError());

	misc_blit_with_center(s, dest, &center);
	SDL_FreeSurface(s);
	stbi_image_free(data);
}

void button_show(struct Button *butt)
{
	char path[100];
	snprintf(path, sizeof path, "buttons/%s/%s/%s",
		(butt->flags & BUTTON_BIG) ? "big" : "small",
		(butt->flags & BUTTON_HORIZONTAL) ? "horizontal" : "vertical",
		(butt->flags & BUTTON_PRESSED) ? "pressed.png" : "normal.png");
	add_image(path, butt->destsurf, butt->center, &butt->width, &butt->height);

	if (butt->imgpath)
		add_image(butt->imgpath, butt->destsurf, butt->center, NULL, NULL);

	if (butt->text) {
		int fontsz = 50;   // adjust this for small buttons if needed

		const char *newln = strchr(butt->text, '\n');
		if (newln) {
			assert(strrchr(butt->text, '\n') == newln);   // no more than one '\n'

			char line1[100];
			snprintf(line1, sizeof line1, "%.*s", (int)(newln - butt->text), butt->text);
			const char *line2 = newln + 1;

			fontsz = (int)(fontsz * 0.7f);
			SDL_Surface *s1 = misc_create_text_surface(line1, black_color, fontsz);
			SDL_Surface *s2 = misc_create_text_surface(line2, black_color, fontsz);

			misc_blit_with_center(
				s1, butt->destsurf, &(SDL_Point){ butt->center.x, butt->center.y - s1->h/2 });
			misc_blit_with_center(
				s2, butt->destsurf, &(SDL_Point){ butt->center.x, butt->center.y + s2->h/2 });

			SDL_FreeSurface(s1);
			SDL_FreeSurface(s2);
		} else {
			SDL_Surface *s = misc_create_text_surface(butt->text, black_color, 50);
			misc_blit_with_center(s, butt->destsurf, &butt->center);
			SDL_FreeSurface(s);
		}
	}
}

static bool mouse_on_button(const SDL_MouseButtonEvent *me, const struct Button *butt)
{
	return fabsf(me->x - butt->center.x) < butt->width/2 &&
			fabsf(me->y - butt->center.y) < butt->height/2;
}

void button_handle_event(const SDL_Event *evt, struct Button *butt)
{
	if ((
		(evt->type == SDL_MOUSEBUTTONDOWN && mouse_on_button(&evt->button, butt)) ||
		(evt->type == SDL_KEYDOWN && evt->key.keysym.scancode == butt->scancode)
	) && !(butt->flags & BUTTON_PRESSED)) {
		butt->flags |= BUTTON_PRESSED;
	} else if ((
		(evt->type == SDL_MOUSEBUTTONUP && mouse_on_button(&evt->button, butt)) ||
		(evt->type == SDL_KEYUP && evt->key.keysym.scancode == butt->scancode)
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
