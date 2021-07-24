#include "listbox.h"
#include <assert.h>
#include "log.h"
#include "misc.h"
#include "mathstuff.h"
#include "../stb/stb_image.h"

// FIXME: copy/pasta button.c
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
	SDL_assert(s->pixels == data);
	return s;
}

static int rows_on_screen(const struct Listbox *lb)
{
	return lb->destsurf->h / lb->bgimg->h;
}

void listbox_init(struct Listbox *lb)
{
	lb->bgimg = load_image("assets/listbox/normal.png");
	lb->selectimg = load_image("assets/listbox/selected.png");
	SDL_assert(lb->selectimg->w == lb->bgimg->w);
	SDL_assert(lb->selectimg->h == lb->bgimg->h);

	lb->redraw = true;
}

void listbox_destroy(const struct Listbox *lb)
{
	stbi_image_free(lb->selectimg->pixels);
	stbi_image_free(lb->bgimg->pixels);
	SDL_FreeSurface(lb->selectimg);
	SDL_FreeSurface(lb->bgimg);
}

void listbox_show(struct Listbox *lb)
{
	if (!lb->redraw)
		return;
	lb->redraw = false;

	SDL_FillRect(lb->destsurf, NULL, 0);
	// FIXME: blitting is slow, called too often
	SDL_BlitSurface(lb->bgimg, NULL, lb->destsurf, &(SDL_Rect){0,0});

	// FIXME: horribly slow
	for (int i = 0; i < min(rows_on_screen(lb), lb->nentries); i++) {
		int y = i*lb->selectimg->h;
		SDL_Surface *img = (i == lb->selectidx) ? lb->selectimg : lb->bgimg;

		SDL_BlitSurface(img, NULL, lb->destsurf, &(SDL_Rect){0,y});
		SDL_Surface *t = misc_create_text_surface(
			lb->entries[i].text, (SDL_Color){0xff,0xff,0xff,0xff}, 20);
		SDL_BlitSurface(t, NULL, lb->destsurf, &(SDL_Rect){ 10, y });
		SDL_FreeSurface(t);

		if (lb->entries[i].hasbuttons) {
			enum ButtonFlags f = BUTTON_TINY;
			struct Button b = {
				.text = lb->buttontexts[0],
				.destsurf = lb->destsurf,
				// FIXME: center x looks wrong in gui
				.center = { lb->destsurf->w - button_width(f)*3/2, y + lb->selectimg->h/2 },
				.flags = f,
			};
			button_show(&b);

			b.text = lb->buttontexts[1];
			b.center.x += button_width(f);
			button_show(&b);
		}
	}
}

static int scancode_to_delta(const SDL_Event *evt, const struct Listbox *lb)
{
	static_assert(sizeof(lb->upscancodes) == sizeof(lb->downscancodes), "");
	static_assert(sizeof(lb->upscancodes[0]) == sizeof(lb->downscancodes[0]), "");

	int sc = misc_handle_scancode(evt->key.keysym.scancode);
	for (int i = 0; i < sizeof(lb->upscancodes)/sizeof(lb->upscancodes[0]); i++) {
		if (lb->upscancodes[i] != 0 && lb->upscancodes[i] == sc)
			return -1;
		if (lb->downscancodes[i] != 0 && lb->downscancodes[i] == sc)
			return +1;
	}
	return 0;
}

void listbox_handle_event(struct Listbox *lb, const SDL_Event *e)
{
	switch(e->type) {
	case SDL_KEYDOWN:
	{
		int d = scancode_to_delta(e, lb);
		if (d != 0) {
			lb->selectidx += d;
			clamp(&lb->selectidx, 0, lb->nentries - 1);
			lb->redraw = true;
		}
		break;
	}
	// TODO: mouse
	default:
		break;
	}
}
