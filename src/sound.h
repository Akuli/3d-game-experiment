#ifndef SOUND_H
#define SOUND_H

/*
This part of the code uses global state because it's a thin wrapper around
SDL_Mixer, and that's all globals. It's also handy to not pass around a sound
effect playing state everywhere.
*/

enum Sound {
	SOUND_JUMP,
	SOUND_SQUEEZE,
	SOUND_POP,
};

void sound_init(void);
void sound_deinit(void);

void sound_play(enum Sound snd);

#endif   // SOUND_H
