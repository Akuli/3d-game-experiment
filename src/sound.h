#ifndef SOUND_H
#define SOUND_H

/*
This part of the code uses global state because it's a thin wrapper around
SDL_Mixer, and that's all globals. It's also handy to not pass around a sound
effect playing state everywhere.
*/

void sound_init(void);
void sound_deinit(void);

// filename pattern may contain ONE "*" wildcard
void sound_play(const char *fnpattern);

#endif   // SOUND_H
