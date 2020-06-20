#ifdef _WIN32
	#include <direct.h>
	#include <windows.h>
#else
	#define _POSIX_C_SOURCE 200809L    // for chdir()
	#include <unistd.h>
#endif

#include <assert.h>
#include <string.h>
#include <time.h>

#include "camera.h"
#include "chooser.h"
#include "game.h"
#include "player.h"
#include "showall.h"
#include "sound.h"
#include "../generated/filelist.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

static void cd_assets(void)
{
#ifdef _WIN32
	// assets directory is in same directory with the exe file, like dlls are
	wchar_t exepath[MAX_PATH];
	int n = GetModuleFileNameW(NULL, exepath, sizeof(exepath)/sizeof(exepath[0]) - 1);
	assert(n >= 0);
	assert(n < sizeof(exepath)/sizeof(exepath[0]));
	exepath[n] = L'\0';
	log_printf("exe file: %ls\n", exepath);

	wchar_t dir[MAX_PATH];
	int ret = _wsplitpath_s(exepath, NULL, 0, dir, sizeof(dir)/sizeof(dir[0]), NULL, 0, NULL, 0);
	if (ret != 0)
		log_printf_abort("_wsplitpath_s failed with path '%ls'", exepath);

	if (_wchdir(dir) != 0) log_printf_abort("_wchcir to '%ls' failed: %s", dir, strerror(errno));
	if (_chdir(ASSETS_DIR) != 0) log_printf_abort("_chcir to '%s' failed: %s", ASSETS_DIR, strerror(errno));

#else
	// assume that current working directory contains assets
	if (chdir(ASSETS_DIR) != 0)
		log_printf_abort("chdir to '%s' failed: %s", ASSETS_DIR, strerror(errno));
#endif
}

int main(int argc, char **argv)
{
	srand(time(NULL));
	cd_assets();

	if (!( argc == 2 && strcmp(argv[1], "--no-sound") == 0 ))
		sound_init();

	if (TTF_Init() == -1)
		log_printf_abort("TTF_Init failed: %s", TTF_GetError());

	SDL_Window *win = SDL_CreateWindow(
		"TODO: title here", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT, 0);
	if (!win)
		log_printf_abort("SDL_CreateWindow failed: %s", SDL_GetError());

	// TODO: pass this to chooser_run() and game_run()?
	SDL_Surface *winsurf = SDL_GetWindowSurface(win);
	if (!winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	while(true){
		const struct EllipsoidPic *plr1pic, *plr2pic;
		const struct Place *pl;
		if (!chooser_run(win, &plr1pic, &plr2pic, &pl))
			break;

		const struct EllipsoidPic *winner = game_run(win, plr1pic, plr2pic, pl);
		if (!winner)
			break;

		const char *winnerpath = filelist_players[winner - player_get_epics(winsurf->format)];
		printf("\n\nwinner path: %s\n\n\n", winnerpath);
	}

	log_printf("cleaning up for successful exit");
	sound_deinit();
	SDL_DestroyWindow(win);
	SDL_Quit();
}
