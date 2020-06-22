#ifdef _WIN32
	#include <direct.h>
	#include <windows.h>
	#define my_chdir _chdir
#else
	#define _POSIX_C_SOURCE 200809L    // for chdir()
	#include <unistd.h>
	#define my_chdir chdir
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


/*
Where are assets and logs?
- on windows, in same place as the .exe file (that's where DLL's are)
- on posix, assume current working directory, because nobody wants to actually
  install this game permanently to their system and they run it with ./game
*/
static void cd_where_everything_is(void)
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
		log_printf_abort("_wsplitpath_s failed with path '%ls': %s", exepath, strerror(errno));

	if (_wchdir(dir) != 0)
		log_printf_abort("_wchdir to '%ls' failed: %s", dir, strerror(errno));
#endif
}

static void cd_assets(void)
{
	if (my_chdir("assets") != 0)
		log_printf_abort("chdir to assets failed: %s", strerror(errno));
}

int main(int argc, char **argv)
{
	cd_where_everything_is();
	log_init();
	cd_assets();

	srand(time(NULL));
	if (!( argc == 2 && strcmp(argv[1], "--no-sound") == 0 ))
		sound_init();
	if (TTF_Init() == -1)
		log_printf_abort("TTF_Init failed: %s", TTF_GetError());

	SDL_Window *wnd = SDL_CreateWindow(
		"3D game experiment", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT, 0);
	if (!wnd)
		log_printf_abort("SDL_CreateWindow failed: %s", SDL_GetError());

	struct Chooser ch;
	chooser_init(&ch, wnd);
	const struct EllipsoidPic *winner;
	const struct Place *pl = &place_list()[0];    // TODO: place chooser
	enum MiscState s = MISC_STATE_CHOOSER;

	while(1) {
		switch(s) {
		case MISC_STATE_CHOOSER:
			log_printf("running chooser");
			s = chooser_run(&ch);
			break;

		case MISC_STATE_PLAY:
			log_printf("playing the game begins");
			s = play_the_game(wnd, ch.playerch[0].epic, ch.playerch[1].epic, &winner, pl);
			break;

		case MISC_STATE_GAMEOVER:
			log_printf("showing game over screen");
			s = game_over(wnd, winner);
			break;

		case MISC_STATE_QUIT:
			log_printf("cleaning up for successful exit");
			chooser_destroy(&ch);
			sound_deinit();
			SDL_DestroyWindow(wnd);
			SDL_Quit();
			return 0;
		}
	}
}
