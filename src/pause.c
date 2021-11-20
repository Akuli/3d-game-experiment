#include "pause.h"
#include <string.h>
#include <SDL2/SDL.h>
#include "button.h"
#include "log.h"
#include "looptimer.h"

static const SDL_Color white_color = { 0xff, 0xff, 0xff, 0xff };

static void on_continue_clicked     (void *state) { *(enum MiscState *)state = MISC_STATE_PLAY; }
static void on_back_to_chooser_clicked(void *state) { *(enum MiscState *)state = MISC_STATE_CHOOSER; }

enum MiscState show_pause_screen(struct SDL_Window *wnd)
{
	SDL_Surface *wndsurf = SDL_GetWindowSurface(wnd);
	if (!wndsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	SDL_Surface *pausedtext = misc_create_text_surface("Paused", white_color, 60);

	enum MiscState state = (enum MiscState)-1;
	enum ButtonFlags flags = 0;
	struct Button playagainbtn = {
		.text = "Continue",
		.flags = flags,
		.scancodes = { SDL_SCANCODE_ESCAPE },
		.destsurf = wndsurf,
		.center = { wndsurf->w/2, wndsurf->h/2 },
		.onclick = on_continue_clicked,
		.onclickdata = &state,
	};
	struct Button back2chooserbtn = {
		.text = "Stop",
		.flags = flags,
		.destsurf = wndsurf,
		.center = {
			playagainbtn.center.x,
			playagainbtn.center.y + button_height(flags),
		},
		.onclick = on_back_to_chooser_clicked,
		.onclickdata = &state,
	};

	button_show(&playagainbtn);
	button_show(&back2chooserbtn);
	misc_blit_with_center(pausedtext, wndsurf, &(SDL_Point){ wndsurf->w/2, wndsurf->h/4 });

	struct LoopTimer lt = {0};
	while(state == (enum MiscState)-1) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			button_handle_event(&e, &playagainbtn);
			button_handle_event(&e, &back2chooserbtn);

			if (e.type == SDL_QUIT) {
				state = MISC_STATE_QUIT;
				goto out;
			}
		}

		SDL_UpdateWindowSurface(wnd);
		looptimer_wait(&lt);
	}

out:
	SDL_FreeSurface(pausedtext);
	return state;
}
