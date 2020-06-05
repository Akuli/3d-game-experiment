#include "sound.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "log.h"

struct Sound {
	const char *filename;
	int volume_percents;
	Mix_Chunk *chunk;
};

static struct Sound sounds[] = {
	// must be sorted by filename, that's used for binary searching the list
	{ "boing.wav", 100, NULL },
	{ "farts/fart1.wav", 100, NULL },
	{ "farts/fart10.wav", 100, NULL },
	{ "farts/fart11.wav", 100, NULL },
	{ "farts/fart2.wav", 100, NULL },
	{ "farts/fart3.wav", 100, NULL },
	{ "farts/fart4.wav", 100, NULL },
	{ "farts/fart5.wav", 100, NULL },
	{ "farts/fart6.wav", 100, NULL },
	{ "farts/fart7.wav", 100, NULL },
	{ "farts/fart8.wav", 100, NULL },
	{ "farts/fart9.wav", 100, NULL },
	{ "lemonsqueeze.wav", 100, NULL },
	{ "pop.wav", 100, NULL },
};

#define CHUNK_SIZE 1024

void sound_init(void)
{
	// check that sounds array is sorted
	for (size_t i = 0; i < sizeof(sounds)/sizeof(sounds[0]) - 1; i++)
		assert(strcmp(sounds[i].filename, sounds[i+1].filename) < 0);

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

	for (size_t i = 0; i < sizeof(sounds)/sizeof(sounds[0]); i++) {
		char path[1024];
		snprintf(path, sizeof path, "sounds/%s", sounds[i].filename);

		assert(!sounds[i].chunk);
		if (!( sounds[i].chunk = Mix_LoadWAV(path) )) {
			log_printf("Mix_LoadWav(\"%s\") failed: %s", path, Mix_GetError());
			continue;
		}
		Mix_VolumeChunk(sounds[i].chunk, MIX_MAX_VOLUME * sounds[i].volume_percents / 100);
	}
}

static bool string_starts_with(const char *s, const char *pre, size_t prelen)
{
	return (strncmp(s, pre, prelen) == 0);
}

static bool string_ends_with(const char *s, const char *suff)
{
	if (strlen(s) < strlen(suff))
		return false;
	return (strcmp(s + strlen(s) - strlen(suff), suff) == 0);
}

static int compare_sound_filename(const void *a, const void *b)
{
	return strcmp(a, ((const struct Sound *)b)->filename);
}

static const struct Sound *choose_sound(const char *pattern)
{
	const char *star = strchr(pattern, '*');
	if (!star) {
		// no wildcard being used, binary search with filename
		return bsearch(
			pattern, sounds, sizeof(sounds)/sizeof(sounds[0]), sizeof(sounds[0]),
			compare_sound_filename);
	}
	assert(strrchr(pattern, '*') == star);   // no more than 1 wildcard

	const struct Sound *matching[sizeof(sounds)/sizeof(sounds[0])];
	int nmatching = 0;

	for (size_t i = 0; i < sizeof(sounds)/sizeof(sounds[0]); i++) {
		if (string_starts_with(sounds[i].filename, pattern, (size_t)(star - pattern)) &&
			string_ends_with(sounds[i].filename, star+1))
		{
			matching[nmatching++] = &sounds[i];
		}
	}

	if (nmatching == 0)
		return NULL;
	return matching[rand() % nmatching];
}

void sound_play(const char *fnpattern)
{
	const struct Sound *snd = choose_sound(fnpattern);

	if (snd == NULL)
		log_printf("no sounds match the pattern '%s'", fnpattern);
	else if (snd->chunk == NULL)
		log_printf("loading sound '%s' has failed", snd->filename);
	else if (Mix_PlayChannel(-1, snd->chunk, 0) == -1)
		log_printf("Mix_PlayChannel for sound '%s' failed: %s", snd->filename, Mix_GetError());
}

void sound_deinit(void)
{
	for (size_t i = 0; i < sizeof(sounds)/sizeof(sounds[0]); i++)
		Mix_FreeChunk(sounds[i].chunk);

	Mix_CloseAudio();
	Mix_Quit();
}
