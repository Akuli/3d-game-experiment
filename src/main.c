#include <assert.h>
#include <string.h>
#include <time.h>

#include "camera.h"
#include "game.h"
#include "showall.h"
#include "sound.h"

int main(int argc, char **argv)
{
	srand(time(NULL));

	if (!( argc == 2 && strcmp(argv[1], "--no-sound") == 0 ))
		sound_init();

	SDL_Window *win = SDL_CreateWindow(
		"TODO: title here", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT, 0);
	if (!win)
		log_printf_abort("SDL_CreateWindow failed: %s", SDL_GetError());

	game_run(win, &place_list()[0]);

	sound_deinit();
	SDL_DestroyWindow(win);
	SDL_Quit();
}
