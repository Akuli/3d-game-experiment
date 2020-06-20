#ifdef _WIN32
	#include <direct.h>
	#include <wnddows.h>
#else
	#define _POSIX_C_SOURCE 200809L    // for chdir()
	#include <unistd.h>
#endif

#include <assert.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "camera.h"
#include "chooser.h"
#include "play.h"
#include "gameover.h"
#include "misc.h"
#include "player.h"
#include "showall.h"
#include "sound.h"
#include "../generated/filelist.h"


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

	SDL_Window *wnd = SDL_CreateWindow(
		"TODO: title here", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT, 0);
	if (!wnd)
		log_printf_abort("SDL_CreateWindow failed: %s", SDL_GetError());

	const struct EllipsoidPic *plr1pic, *plr2pic, *winner;
	const struct Place *pl;
	enum MiscState s = MISC_STATE_CHOOSER;

	while(1) {
		switch(s) {
		case MISC_STATE_CHOOSER:
			log_printf("running chooser");
			s = chooser_run(wnd, &plr1pic, &plr2pic, &pl);
			break;

		case MISC_STATE_PLAY:
			log_printf("playing the game begins");
			s = play_the_game(wnd, plr1pic, plr2pic, &winner, pl);
			break;

		case MISC_STATE_GAMEOVER:
			log_printf("game is over");
			s = game_over(wnd, winner);
			break;

		case MISC_STATE_QUIT:
			log_printf("cleaning up for successful exit");
			sound_deinit();
			SDL_DestroyWindow(wnd);
			SDL_Quit();
			return 0;
		}
	}
}
