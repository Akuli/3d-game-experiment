#include "gameover.h"
#include <string.h>
#include <SDL2/SDL.h>
#include "button.h"
#include "log.h"
#include "looptimer.h"
#include "player.h"

static const SDL_Color white_color = { 0xff, 0xff, 0xff, 0xff };

static void on_play_again_clicked     (void *state) { *(enum MiscState *)state = MISC_STATE_PLAY; }
static void on_back_to_chooser_clicked(void *state) { *(enum MiscState *)state = MISC_STATE_CHOOSER; }

enum MiscState game_over(
	struct SDL_Window *wnd, const struct EllipsoidPic *winnerpic)
{
	SDL_Surface *wndsurf = SDL_GetWindowSurface(wnd);
	if (!wndsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	char msg[100];
	player_epic_name(winnerpic, msg, sizeof(msg)/2);
	strcat(msg, " wins!");
	SDL_Surface *winnertext = misc_create_text_surface(msg, white_color, 60);

	enum MiscState state = MISC_STATE_GAMEOVER;
	enum ButtonFlags flags = BUTTON_BIG;
	struct Button playagainbtn = {
		.text = "Play again",
		.flags = flags,
		.scancodes = { SDL_SCANCODE_F5 },
		.destsurf = wndsurf,
		.center = { wndsurf->w/2, wndsurf->h/2 },
		.onclick = on_play_again_clicked,
		.onclickdata = &state,
	};
	struct Button back2chooserbtn = {
		.text = "Change players\nor place",
		.flags = flags,
		.scancodes = { SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE },
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
	misc_blit_with_center(winnertext, wndsurf, &(SDL_Point){ wndsurf->w/2, wndsurf->h/4 });

	struct LoopTimer lt = {0};
	while(state == MISC_STATE_GAMEOVER) {
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
	SDL_FreeSurface(winnertext);
	return state;
}
