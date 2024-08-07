#include "sound.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "glob.h"
#include "log.h"

struct Sound {
	char name[1024];    // includes "assets/sounds/" prefix
};

static struct Sound sounds[100] = {0};
static int nsounds = 0;

#define CHUNK_SIZE 1024

static int compare_sound_filename(const void *a, const void *b)
{
	const char *astr = a;
	const char *bstr = ((struct Sound *)b)->name;
	return strcmp(astr, bstr);
}

static int compare_sounds(const void *a, const void *b)
{
	const char *astr = ((struct Sound *)a)->name;
	const char *bstr = ((struct Sound *)b)->name;
	return strcmp(astr, bstr);
}

void sound_init(void)
{
	if (SDL_Init(SDL_INIT_AUDIO) == -1) {
		log_printf("SDL_Init(SDL_INIT_AUDIO) failed: %s", SDL_GetError());
		return;
	}

	glob_t gl = {0};
	if (glob("assets/sounds/*.wav", GLOB_APPEND, NULL, &gl) != 0)
		log_printf("can't find non-fart sounds");
	if (glob("assets/sounds/farts/*.wav", GLOB_APPEND, NULL, &gl) != 0)
		log_printf("can't find fart sounds");

	SDL_assert(gl.gl_pathc <= sizeof(sounds)/sizeof(sounds[0]));
	for (int i = 0; i < gl.gl_pathc; i++) {
		snprintf(sounds[nsounds].name, sizeof(sounds[nsounds].name), "%s", gl.gl_pathv[i]);
		nsounds++;
	}

	globfree(&gl);

	qsort(sounds, nsounds, sizeof(sounds[0]), compare_sounds);   // for binary searching
}

void sound_play(const char *fnpattern)
{
	char fullpat[1024];
	snprintf(fullpat, sizeof fullpat, "assets/sounds/%s", fnpattern);

	glob_t gl;
	int r = glob(fullpat, 0, NULL, &gl);
	switch(r) {
		case 0: break;
		case GLOB_NOMATCH: log_printf("no sounds match pattern \"%s\"", fullpat); return;
		case GLOB_NOSPACE: log_printf("glob ran out of memory with pattern \"%s\"", fullpat); return;
		case GLOB_ABORTED: log_printf("glob error with pattern \"%s\": %s", fullpat, strerror(errno)); return;
		default: log_printf("unexpected glob() return value %d with pattern \"%s\"", r, fullpat); return;
	}

	SDL_assert(gl.gl_pathc >= 1);
	const char *path = gl.gl_pathv[rand() % gl.gl_pathc];
	const struct Sound *s = bsearch(path, sounds, nsounds, sizeof(sounds[0]), compare_sound_filename);
	if (s) {
		char command[1024];
		snprintf(command, sizeof(command), "aplay '%s' &", s->name);
		puts(command);
		system(command);
	} else {
		log_printf("sound not loaded: %s", path);
	}

	globfree(&gl);
}

void sound_deinit(void)
{
}
