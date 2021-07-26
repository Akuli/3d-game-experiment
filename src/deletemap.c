#include "deletemap.h"
#include <stdbool.h>
#include <stdio.h>
#include "button.h"
#include "log.h"
#include "looptimer.h"

// TODO: should show preview of map being edited

static void set_to_true(void *ptr)
{
	*(bool *)ptr = true;
}

enum MiscState deletemap_dialog(struct SDL_Window *wnd, struct Map *maps, int *nmaps, int mapidx)
{
	SDL_Surface *wndsurf = SDL_GetWindowSurface(wnd);
	if (!wndsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	SDL_FillRect(wndsurf, NULL, 0);

	char msg[200];
	snprintf(msg, sizeof msg, "Do you really want to delete \"%s\"?", maps[mapidx].name);
	SDL_Surface *tsurf = misc_create_text_surface(msg, (SDL_Color){0xff,0xff,0xff}, 25);
	misc_blit_with_center(tsurf, wndsurf, &(SDL_Point){ wndsurf->w/2, wndsurf->h/4 });
	SDL_FreeSurface(tsurf);

	bool yesclicked = false;
	bool noclicked = false;
	struct Button yesbtn = {
		.text = "Yes, please\ndelete it",
		.destsurf = wndsurf,
		.scancodes = { SDL_SCANCODE_Y },
		.center = { wndsurf->w/2 - button_width(0)/2, wndsurf->h/2 },
		.onclick = set_to_true,
		.onclickdata = &yesclicked,
	};
	struct Button nobtn = {
		.text = "No, don't\ntouch it",
		.scancodes = { SDL_SCANCODE_N, SDL_SCANCODE_ESCAPE },
		.destsurf = wndsurf,
		.center = { wndsurf->w/2 + button_width(0)/2, wndsurf->h/2 },
		.onclick = set_to_true,
		.onclickdata = &noclicked,
	};
	button_show(&yesbtn);
	button_show(&nobtn);

	struct LoopTimer lt = {0};
	while(!yesclicked && !noclicked) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				return MISC_STATE_QUIT;
			button_handle_event(&e, &yesbtn);
			button_handle_event(&e, &nobtn);
		}
		SDL_UpdateWindowSurface(wnd);
		looptimer_wait(&lt);
	}

	if (yesclicked)
		map_delete(maps, nmaps, mapidx);
	return MISC_STATE_CHOOSER;
}
