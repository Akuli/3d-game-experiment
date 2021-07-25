#include "deletemap.h"
#include "button.h"
#include "log.h"
#include "looptimer.h"

static void set_to_true(void *ptr)
{
	*(bool *)ptr = true;
}

enum MiscState deletemap_dialog(struct SDL_Window *wnd, struct Map *maps, int *nmaps, int mapidx)
{
	SDL_Surface *wndsurf = SDL_GetWindowSurface(wnd);
	if (!wndsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	char msg[200];
	snprintf(msg, sizeof msg, "Do you really want to delete \"%s\"?", maps[mapidx].name);
	SDL_FillRect(wndsurf, NULL, 0);
	SDL_Surface *textsurf = misc_create_text_surface(msg, (SDL_Color){0xff,0xff,0xff}, 25);

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
	misc_blit_with_center(textsurf, wndsurf, &(SDL_Point){ wndsurf->w/2, wndsurf->h/4 });

	struct LoopTimer lt = {0};
	enum MiscState ret = MISC_STATE_CHOOSER;
	while(!yesclicked && !noclicked) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				ret = MISC_STATE_QUIT;
				goto out;
			}
			button_handle_event(&e, &yesbtn);
			button_handle_event(&e, &nobtn);
		}
		SDL_UpdateWindowSurface(wnd);
		looptimer_wait(&lt);
	}

	if (yesclicked)
		map_delete(maps, nmaps, mapidx);

out:
	SDL_FreeSurface(textsurf);
	return ret;
}
