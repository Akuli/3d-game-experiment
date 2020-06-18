#include <assert.h>
#include <string.h>
#include <time.h>

#include "camera.h"
#include "chooser.h"
#include "game.h"
#include "showall.h"
#include "sound.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

int main(int argc, char **argv)
{
	srand(time(NULL));

	if (!( argc == 2 && strcmp(argv[1], "--no-sound") == 0 ))
		sound_init();

	if (TTF_Init() == -1)
		log_printf_abort("TTF_Init failed: %s", TTF_GetError());

	SDL_Window *win = SDL_CreateWindow(
		"TODO: title here", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT, 0);
	if (!win)
		log_printf_abort("SDL_CreateWindow failed: %s", SDL_GetError());

	const struct EllipsoidPic *plr1pic, *plr2pic;
	const struct Place *pl;

	while (chooser_run(win, &plr1pic, &plr2pic, &pl) && game_run(win, plr1pic, plr2pic, pl)) { }

	sound_deinit();
	SDL_DestroyWindow(win);
	SDL_Quit();
}
