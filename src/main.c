#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "camera.h"
#include "chooser.h"
#include "enemy.h"
#include "guard.h"
#include "listbox.h"
#include "play.h"
#include "gameover.h"
#include "misc.h"
#include "player.h"
#include "sound.h"
#include "log.h"
#include "map.h"
#include "mapeditor.h"
#include "deletemap.h"

#ifdef _WIN32
	#include <direct.h>
	#include <windows.h>
#endif


/*
Where are assets and logs?
- on windows, in same map as the .exe file (that's where DLL's are)
- on posix, assume current working directory, because nobody wants to actually
  install this game permanently to their system and they run it with ./game
*/
static void cd_where_everything_is(void)
{
#ifdef _WIN32
	// assets directory is in same directory with the exe file, like dlls are
	wchar_t exepath[MAX_PATH];
	int n = GetModuleFileNameW(NULL, exepath, sizeof(exepath)/sizeof(exepath[0]) - 1);
	SDL_assert(n >= 0);
	SDL_assert(n < sizeof(exepath)/sizeof(exepath[0]));
	exepath[n] = L'\0';
	log_printf("exe file: %s\n", windows_to_utf8(exepath));

	wchar_t drive[10] = {0};
	wchar_t dir[MAX_PATH] = {0};
	int ret = _wsplitpath_s(
		exepath,
		drive, sizeof(drive)/sizeof(drive[0]) - 1,
		dir, sizeof(dir)/sizeof(dir[0]) - 1,
		NULL, 0, NULL, 0);
	if (ret != 0)
		log_printf_abort("_wsplitpath_s failed with path '%s': %s", windows_to_utf8(exepath), strerror(errno));

	wchar_t fulldir[sizeof(drive)/sizeof(drive[0]) + sizeof(dir)/sizeof(dir[0])];
	wcscpy(fulldir, drive);
	wcscat(fulldir, dir);

	if (_wchdir(fulldir) != 0)
		log_printf_abort("_wchdir to '%s' failed: %s", windows_to_utf8(fulldir), strerror(errno));
	log_printf("Changed directory: %s", windows_to_utf8(fulldir));
#endif
}

static void show_loading(const char *msg, SDL_Window *wnd, SDL_Surface *wndsurf, int yidx)
{
	log_printf("loading begins: %s", msg);
	int fontsz = 50;
	SDL_Color white = { 0xff, 0xff, 0xff, 0xff };

	SDL_Surface *msgsurf = create_text_surface(msg, white, fontsz);
	SDL_BlitSurface(msgsurf, NULL, wndsurf, &(SDL_Rect){
		fontsz/5, fontsz*yidx,
		123, 456,   // ignored
	});
	SDL_FreeSurface(msgsurf);

	SDL_UpdateWindowSurface(wnd);
}

static void load_the_stuff(SDL_Window *wnd, SDL_Surface *wndsurf, bool sound)
{
	SDL_FillRect(wndsurf, NULL, 0);
	int yidx = 0;

	show_loading("Loading player pictures...", wnd, wndsurf, yidx++);
	player_init_epics(wndsurf->format);

	show_loading("Loading enemy pictures...", wnd, wndsurf, yidx++);
	enemy_init_epics(wndsurf->format);

	show_loading("Loading the guard picture...", wnd, wndsurf, yidx++);
	guard_init_epic(wndsurf->format);

	if (sound) {
		show_loading("Loading sounds...", wnd, wndsurf, yidx++);
		sound_init();
	}

	show_loading("Loading some other stuff...", wnd, wndsurf, yidx++);
}

int main(int argc, char **argv)
{
	bool sound = true, fullscreen = false;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--no-sound"))
			sound = false;
		else if (!strcmp(argv[i], "--fullscreen"))
			fullscreen = true;
		else {
			fprintf(stderr, "Usage: %s [--no-sound] [--fullscreen]\n", argv[0]);
			return 2;
		}
	}

	cd_where_everything_is();
	log_init();

	SDL_Window *wnd = SDL_CreateWindow(
		"3D game experiment", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT, 0);
	if (!wnd)
		log_printf_abort("SDL_CreateWindow failed: %s", SDL_GetError());
	if (fullscreen)
		SDL_SetWindowFullscreen(wnd, SDL_WINDOW_FULLSCREEN);

	srand(time(NULL));
	if (TTF_Init() == -1)
		log_printf_abort("TTF_Init failed: %s", TTF_GetError());

	/*
	On xmonad, consuming these first events changes return value of
	SDL_GetWindowSurface. Rest of the code assumes that it doesn't change,
	because the window isn't meant to be resizable, and according to docs,
	it should only happen on resize.
	*/
	SDL_Event e;
	while (SDL_PollEvent(&e)) { }

	SDL_Surface *wndsurf = SDL_GetWindowSurface(wnd);
	if (!wndsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());
	load_the_stuff(wnd, wndsurf, sound);

	struct Chooser ch;
	chooser_init(&ch, wnd);
	const struct EllipsoidPic *winner;
	enum State s = STATE_MAPEDITOR;

	while(1) {
		switch(s) {
		case STATE_CHOOSER:
			log_printf("running chooser");
			s = chooser_run(&ch);
			break;

		case STATE_PLAY:
			log_printf(
				"playing the game begins with map \"%s\"",
				ch.mapch.maps[ch.mapch.listbox.selectidx].name);
			s = play_the_game(
				wnd, ch.playerch[0].epic, ch.playerch[1].epic, &winner,
				&ch.mapch.maps[ch.mapch.listbox.selectidx]);
			break;

		case STATE_GAMEOVER:
			log_printf("showing game over screen");
			s = game_over(wnd, winner);
			break;

		case STATE_MAPEDITOR:
			log_printf("starting map editor");
			struct MapEditor *ed = mapeditor_new(wndsurf, 0, 1);
			mapeditor_setmap(ed, &ch.mapch.maps[ch.mapch.listbox.selectidx]);
			mapeditor_setplayers(ed, ch.playerch[0].epic, ch.playerch[1].epic);
			s = mapeditor_run(ed, wnd);
			free(ed);
			break;

		case STATE_DELETEMAP:
			log_printf("starting delete map dialog");
			s = deletemap_dialog(
				wnd, ch.mapch.maps, &ch.mapch.nmaps, ch.mapch.listbox.selectidx,
				ch.playerch[0].epic, ch.playerch[1].epic);
			break;

		case STATE_QUIT:
			log_printf("cleaning up for successful exit");
			chooser_destroy(&ch);
			sound_deinit();
			SDL_DestroyWindow(wnd);
			SDL_Quit();
			return 0;
		}
	}
}
