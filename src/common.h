#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <SDL2/SDL.h>

#define nonfatal_error_printf(FMT, ...) fprintf(stderr, "%s:%d: " FMT "\n", __FILE__, __LINE__, __VA_ARGS__)
#define nonfatal_error(MESSAGE) nonfatal_error_printf("%s", (MESSAGE))

#define fatal_error_printf(...) do{ nonfatal_error_printf(__VA_ARGS__); abort(); } while(0)
#define fatal_error(MESSAGE) fatal_error_printf("%s", MESSAGE)
#define fatal_sdl_error(MESSAGE) fatal_error_printf("%s: %s", (MESSAGE), SDL_GetError())

int iclamp(int val, int min, int max);

// why doesn't sdl2 expose its internal struct SDL_FPoint, lol
struct FPoint { float x, y; };

#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))
#define min4(a,b,c,d) min(min(a,b),min(c,d))
#define max4(a,b,c,d) max(max(a,b),max(c,d))

#endif   // COMMON_H
