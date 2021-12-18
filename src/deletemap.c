#include "deletemap.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "button.h"
#include "ellipsoid.h"
#include "log.h"
#include "looptimer.h"
#include "mapeditor.h"

static void set_to_true(void *ptr)
{
	*(bool *)ptr = true;
}

enum State deletemap_dialog(
	struct SDL_Window *wnd, struct Map *maps, int *nmaps, int mapidx,
	const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic)
{
	SDL_Surface *wndsurf = SDL_GetWindowSurface(wnd);
	if (!wndsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	SDL_FillRect(wndsurf, NULL, 0);

	// Put message in middle of space above buttons
	int buttony = button_height(0) * 3/2;
	int msgy = (buttony - button_height(0)/2) / 2;

	char msg[200];
	snprintf(msg, sizeof msg, "Do you really want to delete \"%s\"?", maps[mapidx].name);
	SDL_Surface *tsurf = create_text_surface(msg, (SDL_Color){0xff,0xff,0xff}, 25);
	blit_with_center(tsurf, wndsurf, &(SDL_Point){ wndsurf->w/2, msgy });
	SDL_FreeSurface(tsurf);

	bool yesclicked = false;
	bool noclicked = false;
	struct Button yesbtn = {
		.text = "Yes, please\ndelete it",
		.destsurf = wndsurf,
		.scancodes = { SDL_SCANCODE_Y },
		.center = { wndsurf->w/2 - button_width(0)/2, buttony },
		.onclick = set_to_true,
		.onclickdata = &yesclicked,
	};
	struct Button nobtn = {
		.text = "No, don't\ntouch it",
		.scancodes = { SDL_SCANCODE_N, SDL_SCANCODE_ESCAPE },
		.destsurf = wndsurf,
		.center = { wndsurf->w/2 + button_width(0)/2, buttony },
		.onclick = set_to_true,
		.onclickdata = &noclicked,
	};
	button_show(&yesbtn);
	button_show(&nobtn);

	SDL_Surface *edsurf = create_cropped_surface(wndsurf, (SDL_Rect){
		.x = 0,
		.y = buttony + button_height(0)/2,
		.w = wndsurf->w,
		.h = wndsurf->h - (buttony + button_height(0)/2),
	});
	struct MapEditor *ed = mapeditor_new(edsurf, -100, 0.8f);
	mapeditor_setmap(ed, &maps[mapidx]);
	mapeditor_setplayers(ed, plr0pic, plr1pic);

	enum State ret;
	struct LoopTimer lt = {0};
	while(!yesclicked && !noclicked) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				ret = STATE_QUIT;
				goto out;
			}
			button_handle_event(&e, &yesbtn);
			button_handle_event(&e, &nobtn);
		}
		mapeditor_displayonly_eachframe(ed);
		SDL_UpdateWindowSurface(wnd);
		looptimer_wait(&lt);
	}

	if (yesclicked)
		map_delete(maps, nmaps, mapidx);
	ret = STATE_CHOOSER;

out:
	SDL_FreeSurface(edsurf);
	free(ed);
	return ret;
}
