#include "listbox.h"
#include <assert.h>
#include <stdlib.h>
#include "log.h"
#include "misc.h"
#include "mathstuff.h"

void listbox_init(struct Listbox *lb)
{
	lb->bgimg = misc_create_image_surface("assets/listbox/normal.png");
	lb->selectimg = misc_create_image_surface("assets/listbox/selected.png");

	SDL_assert(lb->destrect.w == LISTBOX_WIDTH);
	SDL_assert(lb->selectimg->w == LISTBOX_WIDTH);
	SDL_assert(lb->bgimg->w == LISTBOX_WIDTH);
	SDL_assert(lb->selectimg->h == lb->bgimg->h);

	int fit = lb->destrect.h / lb->bgimg->h;
	lb->visiblebuttons = malloc(fit*sizeof( ((struct ListboxEntry *)NULL)->buttons ));
	if (!lb->visiblebuttons)
		log_printf_abort("out of mem");
	lb->redraw = true;
}

void listbox_destroy(const struct Listbox *lb)
{
	misc_free_image_surface(lb->selectimg);
	misc_free_image_surface(lb->bgimg);
	free(lb->visiblebuttons);
}

static int get_button_center_y(const struct Listbox *lb, int row)
{
	return lb->destrect.y + (row - lb->firstvisible)*lb->selectimg->h + lb->selectimg->h/2;
}

void listbox_show(struct Listbox *lb)
{
	if (!lb->redraw)
		return;
	lb->redraw = false;

	int nentries = 0;
	while (lb->getentry(lb->cbdata, nentries))
		nentries++;

	int fit = lb->destrect.h / lb->bgimg->h;
	if (fit >= nentries)
		lb->firstvisible = 0;
	else {
		lb->firstvisible = lb->selectidx - fit/2;
		clamp(&lb->firstvisible, 0, nentries - fit);
	}

	SDL_FillRect(lb->destsurf, &lb->destrect, 0);
	SDL_BlitSurface(lb->bgimg, NULL, lb->destsurf, &(SDL_Rect){ lb->destrect.x, lb->destrect.y });

	lb->nvisiblebuttons = 0;
	int topy = lb->destrect.y;

	// horribly slow, but doesn't run very often
	for (int i = lb->firstvisible; i < min(lb->firstvisible + fit, nentries); i++) {
		const struct ListboxEntry *e = lb->getentry(lb->cbdata, i);
		SDL_Surface *img = (i == lb->selectidx) ? lb->selectimg : lb->bgimg;

		SDL_Surface *t = misc_create_text_surface(e->text, (SDL_Color){0xff,0xff,0xff,0xff}, 20);
		SDL_BlitSurface(img, NULL, lb->destsurf, &(SDL_Rect){lb->destrect.x, topy});
		SDL_BlitSurface(t, NULL, lb->destsurf, &(SDL_Rect){lb->destrect.x + 10, topy});
		SDL_FreeSurface(t);

		int centerx = lb->destrect.x + lb->destrect.w - button_width(BUTTON_TINY)/2;
		for (int k = sizeof(e->buttons)/sizeof(e->buttons[0]) - 1; k >= 0; k--) {
			if (e->buttons[k].text) {
				struct Button *ptr = &lb->visiblebuttons[lb->nvisiblebuttons++];
				*ptr = e->buttons[k];
				ptr->destsurf = lb->destsurf;
				ptr->flags |= BUTTON_TINY;
				ptr->center = (SDL_Point){ centerx, get_button_center_y(lb, i) };
				button_show(ptr);
			}
			centerx -= button_width(BUTTON_TINY);
		}
		topy += lb->selectimg->h;
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

static void select_index(struct Listbox *lb, int i)
{
	if (i != lb->selectidx && lb->getentry(lb->cbdata, i) != NULL) {
		lb->selectidx = i;
		lb->redraw = true;
	}
}

static void move_to_index(struct Listbox *lb, int i)
{
	if (i != lb->selectidx
		&& lb->getentry(lb->cbdata, i)
		&& lb->getentry(lb->cbdata, lb->selectidx)->movable
		&& lb->move(lb->cbdata, lb->selectidx, i))
	{
		lb->selectidx = i;
		lb->redraw = true;
	}
}

void listbox_handle_event(struct Listbox *lb, const SDL_Event *e)
{
	switch(e->type) {
	case SDL_KEYDOWN:
	{
		bool lshift = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LSHIFT];
		bool rshift = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_RSHIFT];
		if (lshift || rshift)
			move_to_index(lb, lb->selectidx + scancode_to_delta(e, lb));
		else
			select_index(lb, lb->selectidx + scancode_to_delta(e, lb));
		break;
	}

	case SDL_MOUSEBUTTONDOWN:
		if (SDL_PointInRect(&(SDL_Point){e->button.x, e->button.y}, &lb->destrect)) {
			select_index(lb, (e->button.y - lb->destrect.y)/lb->selectimg->h);
			lb->mousedragging = true;
		}
		break;

	case SDL_MOUSEBUTTONUP:
		lb->mousedragging = false;
		break;

	case SDL_MOUSEMOTION:
		if (lb->mousedragging)
			move_to_index(lb, (e->button.y - lb->destrect.y)/lb->selectimg->h);
		break;

	default:
		break;
	}

	for (int i = 0; i < lb->nvisiblebuttons; i++) {
		if (lb->visiblebuttons[i].center.y == get_button_center_y(lb, lb->selectidx))
			button_handle_event(e, &lb->visiblebuttons[i]);
	}
}
