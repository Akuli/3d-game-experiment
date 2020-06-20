#include "button.h"
#include "log.h"
#include "misc.h"
#include "../stb/stb_image.h"

static const SDL_Color black_color = { 0x00, 0x00, 0x00, 0xff };

static SDL_Surface *create_image_surface(const char *path)
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
	return s;
}

void button_refresh(struct Button *butt)
{
	if (butt->cachesurf)
		SDL_FreeSurface(butt->cachesurf);

	char path[100];
	snprintf(path, sizeof path, "buttons/%s/%s/%s",
		butt->big ? "big" : "small",
		butt->horizontal ? "horizontal" : "vertical",
		butt->pressed ? "pressed.png" : "normal.png");
	butt->cachesurf = create_image_surface(path);

	if (butt->imgpath) {
		SDL_Surface *s = create_image_surface(butt->imgpath);
		misc_blit_with_center(s, butt->cachesurf, NULL);
		SDL_FreeSurface(s);
	}

	if (butt->text) {
		SDL_Surface *s = misc_create_text_surface(butt->text, black_color, 50);
		misc_blit_with_center(s, butt->cachesurf, NULL);
		SDL_FreeSurface(s);
	}
}

void button_show(const struct Button *butt)
{
	misc_blit_with_center(butt->cachesurf, butt->destsurf, &butt->center);
}

static bool mouse_on_button(const SDL_MouseButtonEvent *me, const struct Button *butt)
{
	return fabsf(me->x - butt->center.x) < butt->cachesurf->w/2 &&
			fabsf(me->y - butt->center.y) < butt->cachesurf->h/2;
}

void button_handle_event(const SDL_Event *evt, struct Button *butt)
{
	if ((
		(evt->type == SDL_MOUSEBUTTONDOWN && mouse_on_button(&evt->button, butt)) ||
		(evt->type == SDL_KEYDOWN && evt->key.keysym.scancode == butt->scancode)
	) && !butt->pressed) {
		butt->pressed = true;
	} else if ((
		(evt->type == SDL_MOUSEBUTTONUP && mouse_on_button(&evt->button, butt)) ||
		(evt->type == SDL_KEYUP && evt->key.keysym.scancode == butt->scancode)
	) && butt->pressed) {
		butt->pressed = false;
		butt->onclick(butt->onclickdata);
	} else if (evt->type == SDL_MOUSEBUTTONUP && butt->pressed) {
		// if button has been pressed and mouse has been moved away, unpress button but don't click
		butt->pressed = false;
	} else {
		// don't refresh
		return;
	}
	button_refresh(butt);
}
