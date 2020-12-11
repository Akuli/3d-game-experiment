#include "sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "glob.h"
#include "log.h"

struct Sound {
	Mix_Chunk *chunk;
	char name[1024];    // includes "sounds/" prefix
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

	/*
	wav is supported by default, add other flags here as needed. I don't check the
	return value of Mix_Init because I found this problem that it has had only a
	couple years ago: https://stackoverflow.com/q/52131807
	*/
	Mix_Init(0);

	/*
	Make sure that we can play all the needed sounds at the same time, even
	when smashing buttons. With 20 channels, I could barely smash buttons fast
	enough to run out of channels, but I couldn't do that with 25 channels. The
	default seems to be 16 channels.
	*/
	Mix_AllocateChannels(32);

	if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, CHUNK_SIZE) == -1)
	{
		log_printf("Mix_OpenAudio failed: %s", Mix_GetError());
		return;
	}

	glob_t gl = {0};
	if (glob("sounds/*.wav", GLOB_APPEND, NULL, &gl) != 0)
		log_printf("can't find non-fart sounds");
	if (glob("sounds/farts/*.wav", GLOB_APPEND, NULL, &gl) != 0)
		log_printf("can't find fart sounds");

	for (int i = 0; i < gl.gl_pathc; i++) {
		SDL_assert(sounds[nsounds].chunk == NULL);
		if (( sounds[nsounds].chunk = Mix_LoadWAV(gl.gl_pathv[i]) )) {
			snprintf(sounds[nsounds].name, sizeof(sounds[nsounds].name), "%s", gl.gl_pathv[i]);
			nsounds++;
			SDL_assert(nsounds < sizeof(sounds)/sizeof(sounds[0]));
		} else {
			log_printf("Mix_LoadWav(\"%s\") failed: %s", gl.gl_pathv[i], Mix_GetError());
		}
	}
	globfree(&gl);

	qsort(sounds, nsounds, sizeof(sounds[0]), compare_sounds);   // for binary searching
}

static Mix_Chunk *choose_sound(const char *pattern)
{
	char fullpat[1024];
	snprintf(fullpat, sizeof fullpat, "sounds/%s", pattern);

	glob_t gl;
	switch(glob(fullpat, 0, NULL, &gl)) {
		case 0:
			break;
		case GLOB_NOMATCH:
			log_printf("no sounds match pattern \"%s\"", pattern);
			return NULL;
		case GLOB_NOSPACE:
			log_printf("no sounds match pattern \"%s\"", pattern);
			return NULL;
		default:
			log_printf("unexpected glob() return value with pattern \"%s\"", pattern);
			return NULL;
	}

	SDL_assert(gl.gl_pathc >= 1);
	const char *path = gl.gl_pathv[rand() % gl.gl_pathc];
	const struct Sound *s = bsearch(path, sounds, nsounds, sizeof(sounds[0]), compare_sound_filename);

	if (s)
		log_printf("playing soon: %s", path);
	else
		log_printf("sound not loaded: %s", path);
	globfree(&gl);

	return s ? s->chunk : NULL;
}

void sound_play(const char *fnpattern)
{
	Mix_Chunk *c = choose_sound(fnpattern);
	if (c != NULL && Mix_PlayChannel(-1, c, 0) == -1)
		log_printf("Mix_PlayChannel failed: %s", Mix_GetError());
}

void sound_deinit(void)
{
	for (int i = 0; i < nsounds; i++)
		Mix_FreeChunk(sounds[i].chunk);
	Mix_CloseAudio();
	Mix_Quit();
}
