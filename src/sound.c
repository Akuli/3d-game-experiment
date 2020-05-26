#include "sound.h"
#include <assert.h>
#include <stdbool.h>
#include <SDL2/SDL_mixer.h>
#include "common.h"

#define nonfatal_mix_error(WHATFAILED) nonfatal_error(WHATFAILED, Mix_GetError())

struct {
	const char *filename;
	int volume_percents;
} const sound_infos[] = {
	[SOUND_JUMP] = { "jump.wav", 100 },
	[SOUND_SQUEEZE] = {"squeeze.wav", 100 },
	[SOUND_POP] = { "pop.wav", 100 },
};
#define HOW_MANY_SOUNDS ( sizeof(sound_infos)/sizeof(sound_infos[0]) )
static Mix_Chunk *samples[HOW_MANY_SOUNDS] = {0};

#define CHUNK_SIZE 1024

void sound_init(void)
{
	if (SDL_Init(SDL_INIT_AUDIO) == -1) {
		nonfatal_sdl_error("SDL_Init(SDL_INIT_AUDIO)");
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
		nonfatal_sdl_error("Mix_OpenAudio");
		return;
	}

	for (size_t i = 0; i < HOW_MANY_SOUNDS; i++) {
		assert(sound_infos[i].filename);
		assert(!samples[i]);

		char path[1024];
		snprintf(path, sizeof path, "sounds/%s", sound_infos[i].filename);
		if (!( samples[i] = Mix_LoadWAV(path) )) {
			// TODO: add printf formatting to common.h error functions
			char msg[sizeof path + 100];
			snprintf(msg, sizeof msg, "Mix_LoadWAV(\"%s\")", path);
			nonfatal_mix_error(msg);
			continue;
		}

		Mix_VolumeChunk(samples[i], MIX_MAX_VOLUME * sound_infos[i].volume_percents / 100);
	}
}

void sound_play(enum Sound snd)
{
	if (samples[snd] == NULL) {
		// TODO: add printf formatting to common.h error functions
		char msg[100];
		snprintf(msg, sizeof msg, "loading sound from '%s' has failed", sound_infos[snd].filename);
		nonfatal_error("sound_play", msg);
		return;
	}

	if (Mix_PlayChannel(-1, samples[snd], 0) == -1) {
		char msg[100];
		snprintf(msg, sizeof msg, "Mix_PlayChannel for sound '%s'", sound_infos[snd].filename);
		nonfatal_mix_error(msg);
	}
}

void sound_deinit(void)
{
	for (size_t i = 0; i < HOW_MANY_SOUNDS; i++)
		Mix_FreeChunk(samples[i]);

	Mix_CloseAudio();
	Mix_Quit();
}
