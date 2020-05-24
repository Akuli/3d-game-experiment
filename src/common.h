#ifndef COMMON_H
#define COMMON_H

#include <stdnoreturn.h>
#include <SDL2/SDL.h>

noreturn void fatal_error_internal(
	const char *file, long lineno, const char *whatfailed, const char *msg);
#define fatal_error(WHATFAILED, MSG) fatal_error_internal(__FILE__, __LINE__, (WHATFAILED), (MSG))
#define fatal_sdl_error(WHATFAILED) fatal_error(WHATFAILED, SDL_GetError())

int iclamp(int val, int min, int max);

#endif   // COMMON_H
