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

static int rows_on_screen(const struct Listbox *lb)
{
	return min(lb->destrect.h / lb->bgimg->h, lb->nentries);
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

static void click_first(void *lbptr)
{
	struct Listbox *lb = lbptr;
	lb->clicktext = lb->entries[lb->selectidx].buttontexts[0];
}

static void click_second(void *lbptr)
{
	struct Listbox *lb = lbptr;
	lb->clicktext = lb->entries[lb->selectidx].buttontexts[1];
}

void listbox_show(struct Listbox *lb)
{
	if (!lb->redraw)
		return;
	lb->redraw = false;

	SDL_FillRect(lb->destsurf, &lb->destrect, 0);
	SDL_BlitSurface(lb->bgimg, NULL, lb->destsurf, &(SDL_Rect){ lb->destrect.x, lb->destrect.y });

	// FIXME: horribly slow
	for (int i = 0; i < rows_on_screen(lb); i++) {
		int y = lb->destrect.y + i*lb->selectimg->h;
		SDL_Surface *img = (i == lb->selectidx) ? lb->selectimg : lb->bgimg;

		SDL_BlitSurface(img, NULL, lb->destsurf, &(SDL_Rect){ lb->destrect.x, y });
		SDL_Surface *t = misc_create_text_surface(
			lb->entries[i].text, (SDL_Color){0xff,0xff,0xff,0xff}, 20);
		SDL_BlitSurface(t, NULL, lb->destsurf, &(SDL_Rect){ lb->destrect.x + 10, y });
		SDL_FreeSurface(t);

		static_assert(sizeof(lb->entries[0].buttontexts) / sizeof(lb->entries[0].buttontexts[0]) == 2, "");
		static_assert(sizeof(lb->entries[0].buttons) / sizeof(lb->entries[0].buttons[0]) == 2, "");
		int centerx = lb->destrect.x + lb->destrect.w - button_width(BUTTON_TINY)/2;
		for (int k = 1; k >= 0; k--) {
			if (lb->entries[i].buttontexts[k]) {
				lb->entries[i].buttons[k] = (struct Button){
					.text = lb->entries[i].buttontexts[k],
					.destsurf = lb->destsurf,
					.flags = lb->entries[i].buttons[k].flags | BUTTON_TINY,
					.center = (SDL_Point){ centerx, y + lb->selectimg->h/2 },
					.onclick = k ? click_second : click_first,
					.onclickdata = lb,
				};
				button_show(&lb->entries[i].buttons[k]);
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

const char *listbox_handle_event(struct Listbox *lb, const SDL_Event *e)
{
	switch(e->type) {
	case SDL_KEYDOWN:
		select_index(lb, lb->selectidx + scancode_to_delta(e, lb), true);
		break;

	case SDL_MOUSEBUTTONDOWN:
		if (SDL_PointInRect(&(SDL_Point){e->button.x, e->button.y}, &lb->destrect))
			select_index(lb, (e->button.y - lb->destrect.y)/lb->selectimg->h, false);

	default:
		break;
	}

	for (int i = 0; i < rows_on_screen(lb); i++) {
		for (int k = 0; k < 2; k++) {
			if (lb->entries[i].buttontexts[k])
				button_handle_event(e, &lb->entries[i].buttons[k]);
		}
	}

	const char *ret = lb->clicktext;
	lb->clicktext = NULL;
	return ret;
}
