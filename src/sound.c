#include "sound.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../generated/filelist.h"
#include "log.h"

static bool string_starts_with(const char *s, const char *pre, int prelen)
{
	if (prelen < 0)
		prelen = strlen(pre);
	return (strncmp(s, pre, prelen) == 0);
}

static bool string_ends_with(const char *s, const char *suff)
{
	if (strlen(s) < strlen(suff))
		return false;
	return (strcmp(s + strlen(s) - strlen(suff), suff) == 0);
}

#define N_SOUNDS ( sizeof(filelist_sounds)/sizeof(filelist_sounds[0]) )
static Mix_Chunk *sound_chunks[N_SOUNDS] = {0};

#define CHUNK_SIZE 1024

void sound_init(void)
{
	// filenames must be sorted for binary searching
	for (int i = 0; i < N_SOUNDS-1; i++)
		assert(strcmp(filelist_sounds[i], filelist_sounds[i+1]) < 0);

	// sound_play() takes filename without "sounds/" in front
	for (int i = 0; i < N_SOUNDS; i++)
		assert(string_starts_with(filelist_sounds[i], "sounds/", -1));

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

	if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, CHUNK_SIZE) == -1)
	{
		log_printf("Mix_OpenAudio failed: %s", Mix_GetError());
		return;
	}

	/*
	Make sure that we can play all the needed sounds at the same time, even
	when smashing buttons. With 20 channels, I could barely smash buttons fast
	enough to run out of channels, but I couldn't do that with 25 channels. The
	default seems to be 16 channels.
	*/
	Mix_AllocateChannels(32);

	for (int i = 0; i < N_SOUNDS; i++) {
		assert(sound_chunks[i] == NULL);
		if (!( sound_chunks[i] = Mix_LoadWAV(filelist_sounds[i]) ))
			log_printf("Mix_LoadWav(\"%s\") failed: %s", filelist_sounds[i], Mix_GetError());
	}
}

static int compare_sound_filename(const void *a, const void *b)
{
	const char *astr = a;
	const char *bstr = *(const char *const*)b;

	assert(!string_starts_with(astr, "sounds/", -1));
	assert(string_starts_with(bstr, "sounds/", -1));
	return strcmp(astr, bstr + strlen("sounds/"));
}

static Mix_Chunk *choose_sound(const char *pattern)
{
	const char *star = strchr(pattern, '*');
	if (!star) {
		// no wildcard being used, binary search with filename
		const char *const *ptr = bsearch(
			pattern, filelist_sounds, N_SOUNDS, sizeof(filelist_sounds[0]),
			compare_sound_filename);
		if (!ptr)
			return NULL;

		int i = ptr - filelist_sounds;
		assert(0 <= i && i < N_SOUNDS);
		return sound_chunks[i];
	}

	assert(strrchr(pattern, '*') == star);   // no more than 1 wildcard

	Mix_Chunk *matching[N_SOUNDS];
	int nmatching = 0;

	for (int i = 0; i < N_SOUNDS; i++) {
		const char *fnam = filelist_sounds[i] + strlen("sounds/");
		if (string_starts_with(fnam, pattern, star - pattern) &&
			string_ends_with(fnam, star+1) &&
			sound_chunks[i] != NULL)
		{
			matching[nmatching++] = sound_chunks[i];
		}
	}

	if (nmatching == 0)
		return NULL;
	return matching[rand() % nmatching];
}

void sound_play(const char *fnpattern)
{
	Mix_Chunk *c = choose_sound(fnpattern);
	if (!c)
		log_printf("no sounds match the pattern '%s'", fnpattern);
	else if (Mix_PlayChannel(-1, c, 0) == -1)
		log_printf("Mix_PlayChannel failed: %s", Mix_GetError());
}

void sound_deinit(void)
{
	for (int i = 0; i < N_SOUNDS; i++)
		Mix_FreeChunk(sound_chunks[i]);

	Mix_CloseAudio();
	Mix_Quit();
}
