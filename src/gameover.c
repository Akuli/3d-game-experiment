#include "gameover.h"
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
	snprintf(msg, sizeof msg, "%s wins!", player_getname(winnerpic));
	SDL_Surface *winnertext = misc_create_text_surface(msg, white_color, 60);

	enum MiscState state = MISC_STATE_GAMEOVER;
	struct Button playagainbtn = {
		.text = "Play again",
		.big = true,
		.horizontal = true,
		.scancode = SDL_SCANCODE_F5,
		.destsurf = wndsurf,
		.center = { wndsurf->w/2, wndsurf->h/2 },
		.onclick = on_play_again_clicked,
		.onclickdata = &state,
	};
	struct Button back2chooserbtn = {
		.text = "Change players\nor place",
		.big = true,
		.horizontal = true,
		.scancode = SDL_SCANCODE_RETURN,
		.destsurf = wndsurf,
		.center = { playagainbtn.center.x, playagainbtn.center.y + BUTTON_SIZE_BIG },
		.onclick = on_back_to_chooser_clicked,
		.onclickdata = &state,
	};

	button_refresh(&playagainbtn);
	button_refresh(&back2chooserbtn);

	static struct LoopTimer lt = {0};

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

		// the surface isn't cleared, whatever was left from playing is displayed there
		misc_blit_with_center(winnertext, wndsurf, &(SDL_Point){ wndsurf->w/2, wndsurf->h/4 });
		button_show(&playagainbtn);
		button_show(&back2chooserbtn);

		SDL_UpdateWindowSurface(wnd);
		looptimer_wait(&lt);
	}

out:
	SDL_FreeSurface(winnertext);
	button_destroy(&playagainbtn);
	return state;
}
