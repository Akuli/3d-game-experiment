#ifndef SOUND_H
#define SOUND_H

/*
This part of the code uses global state because it's a thin wrapper around
SDL_Mixer, and that's all globals. It's also handy to not pass around a sound
effect playing state everywhere.
*/

void sound_init(void);
void sound_play(const char *filename);
void sound_deinit(void);

#endif   // SOUND_H
