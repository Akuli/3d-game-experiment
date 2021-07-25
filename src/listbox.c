#include "listbox.h"
#include <assert.h>
#include <stdlib.h>
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

void listbox_init(struct Listbox *lb)
{
	lb->bgimg = load_image("assets/listbox/normal.png");
	lb->selectimg = load_image("assets/listbox/selected.png");

	SDL_assert(lb->destrect.w == LISTBOX_WIDTH);
	SDL_assert(lb->selectimg->w == LISTBOX_WIDTH);
	SDL_assert(lb->bgimg->w == LISTBOX_WIDTH);
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

	int fit = lb->destrect.h / lb->bgimg->h;
	if (fit >= lb->nentries)
		lb->firstvisible = 0;
	else {
		lb->firstvisible = lb->selectidx - fit/2;
		clamp(&lb->firstvisible, 0, lb->nentries - fit);
	}

	SDL_FillRect(lb->destsurf, &lb->destrect, 0);
	SDL_BlitSurface(lb->bgimg, NULL, lb->destsurf, &(SDL_Rect){ lb->destrect.x, lb->destrect.y });

	// horribly slow, but doesn't run very often
	for (struct ListboxEntry *e = &lb->entries[lb->firstvisible];
		e < &lb->entries[min(lb->firstvisible + fit, lb->nentries)];
		e++)
	{
		int y = lb->destrect.y + (e - &lb->entries[lb->firstvisible])*lb->selectimg->h;
		SDL_Surface *img = (e == &lb->entries[lb->selectidx]) ? lb->selectimg : lb->bgimg;

		SDL_Surface *t = misc_create_text_surface(e->text, (SDL_Color){0xff,0xff,0xff,0xff}, 20);
		SDL_BlitSurface(img, NULL, lb->destsurf, &(SDL_Rect){lb->destrect.x, y});
		SDL_BlitSurface(t, NULL, lb->destsurf, &(SDL_Rect){lb->destrect.x + 10, y});
		SDL_FreeSurface(t);

		int centerx = lb->destrect.x + lb->destrect.w - button_width(BUTTON_TINY)/2;
		for (int k = sizeof(e->buttons)/sizeof(e->buttons[0]) - 1; k >= 0; k--) {
			if (e->buttons[k].text) {
				e->buttons[k].destsurf = lb->destsurf;
				e->buttons[k].flags |= BUTTON_TINY;
				e->buttons[k].center = (SDL_Point){ centerx, y + lb->selectimg->h/2 };
				button_show(&e->buttons[k]);
			}
			centerx -= button_width(BUTTON_TINY);
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

static void select_index(struct Listbox *lb, int i, bool wantclamp)
{
	if (wantclamp)
		clamp(&i, 0, lb->nentries - 1);

	if (0 <= i && i < lb->nentries && i != lb->selectidx) {
		lb->selectidx = i;
		lb->redraw = true;
	}
}

void listbox_handle_event(struct Listbox *lb, const SDL_Event *e)
{
	switch(e->type) {
	case SDL_KEYDOWN:
		select_index(lb, lb->selectidx + scancode_to_delta(e, lb), true);
		break;

	case SDL_MOUSEBUTTONDOWN:
		if (SDL_PointInRect(&(SDL_Point){e->button.x, e->button.y}, &lb->destrect))
			select_index(lb, (e->button.y - lb->destrect.y)/lb->selectimg->h, false);
		break;

	default:
		break;
	}

#define ArrayLen(arr) (sizeof((arr))/sizeof((arr)[0]))
	for (int k = 0; k < ArrayLen(lb->entries[0].buttons); k++) {
#undef ArrayLen
		if (lb->entries[lb->selectidx].buttons[k].text)
			button_handle_event(e, &lb->entries[lb->selectidx].buttons[k]);  // may reallocate lb->entries
	}
}
