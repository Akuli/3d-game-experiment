#include "sound.h"
#include <assert.h>
#include <stdlib.h>
#include <SDL2/SDL_mixer.h>
#include "common.h"

#define nonfatal_mix_error_printf(FMT, ...) nonfatal_error_printf(FMT ": %s", __VA_ARGS__, Mix_GetError())
#define nonfatal_mix_error(MESSAGE) nonfatal_mix_error_printf("%s", (MESSAGE))

struct Sound {
	const char *filename;
	int volume_percents;
	Mix_Chunk *chunk;
};

static struct Sound sounds[] = {
	// must be sorted by filename, that's used for binary searching the list
	{ "boing.wav", 100, NULL },
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
		nonfatal_error_printf("SDL_Init(SDL_INIT_AUDIO) failed: %s", SDL_GetError());
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
		nonfatal_mix_error("Mix_OpenAudio failed");
		return;
	}

	/*
	Make sure that we can play all the needed sounds at the same time, even
	when smashing buttons. With 20 channels, I could barely smash buttons fast
	enough to run out of channels, but I couldn't do that with 25 channels.
	*/
	Mix_AllocateChannels(32);

	for (size_t i = 0; i < sizeof(sounds)/sizeof(sounds[0]); i++) {
		char path[1024];
		snprintf(path, sizeof path, "sounds/%s", sounds[i].filename);

		assert(!sounds[i].chunk);
		if (!( sounds[i].chunk = Mix_LoadWAV(path) )) {
			nonfatal_mix_error_printf("Mix_LoadWav(\"%s\") failed", path);
			continue;
		}
		Mix_VolumeChunk(sounds[i].chunk, MIX_MAX_VOLUME * sounds[i].volume_percents / 100);
	}
}

static int compare_sound_filename(const void *a, const void *b)
{
	return strcmp(a, ((const struct Sound *)b)->filename);
}

void sound_play(const char *filename)
{
	const struct Sound *snd = bsearch(
		filename, sounds, sizeof(sounds)/sizeof(sounds[0]), sizeof(sounds[0]),
		compare_sound_filename);

	if (snd == NULL)
		nonfatal_error_printf("sound '%s' not found", filename);
	else if (snd->chunk == NULL)
		nonfatal_error_printf("loading sound '%s' has failed", filename);
	else if (Mix_PlayChannel(-1, snd->chunk, 0) == -1)
		nonfatal_mix_error_printf("Mix_PlayChannel for sound '%s' failed", filename);
}

void sound_deinit(void)
{
	for (size_t i = 0; i < sizeof(sounds)/sizeof(sounds[0]); i++)
		Mix_FreeChunk(sounds[i].chunk);

	Mix_CloseAudio();
	Mix_Quit();
}
