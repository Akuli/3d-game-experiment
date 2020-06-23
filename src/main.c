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
#include "showall.h"
#include "sound.h"

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

	if (_wchdir(dir) != 0) log_printf_abort("_whcir(L\"%ls\") failed: %s", dir, strerror(errno));
	if (_wchdir(L"assets") != 0) log_printf_abort("_whcir(L\"assets\") failed: %s", strerror(errno));

#else
	// assume that the game isn't installed anywhere
	if (chdir("assets") != 0)
		log_printf_abort("chdir(\"assets\") failed: %s", strerror(errno));
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

	const struct EllipsoidPic *plr1pic, *plr2pic;
	const struct Place *pl;

	while (chooser_run(win, &plr1pic, &plr2pic, &pl) && game_run(win, plr1pic, plr2pic, pl)) { }

	sound_deinit();
	SDL_DestroyWindow(win);
	SDL_Quit();
}
