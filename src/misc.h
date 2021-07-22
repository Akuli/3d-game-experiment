#ifndef MISC_H
#define MISC_H

#include <SDL2/SDL.h>
#include <stdint.h>

enum MiscState {
	MISC_STATE_CHOOSER,
	MISC_STATE_MAPEDITOR,
	MISC_STATE_PLAY,
	MISC_STATE_GAMEOVER,
	MISC_STATE_QUIT,
};

// logs errors, won't work with non-ascii paths on windows
void misc_mkdir(const char *path);

/*
In this game, we don't want to distinguish some scancodes from each other to allow
multiple ways to do the same thing. For all different scan codes that should do the same
thing, this function returns the same scancode.
*/
int misc_handle_scancode(int sc);

/*
Blit src onto dst so that center of src goes to *center. If center is NULL, then
center of src goes to center of dst.
*/
void misc_blit_with_center(SDL_Surface *src, SDL_Surface *dst, const SDL_Point *center);

// Return a surface containing text on transparent background. Never returns NULL.
SDL_Surface *misc_create_text_surface(const char *text, SDL_Color col, int fontsz);

/*
Create a surface that refers to another another surface. So, drawing to the
returned surface actually draws to the surface given as argument. This turns out
to be much faster than blitting. Never returns NULL.
*/
SDL_Surface *misc_create_cropped_surface(SDL_Surface *surf, SDL_Rect r);

// 24 rightmost bits used, 8 bits for each of R,G,B
// very perf critical in wall drawing code, especially in places with lots of walls
inline uint32_t misc_rgb_average(uint32_t a, uint32_t b) {
	return ((a & 0xfefefe) >> 1) + ((b & 0xfefefe) >> 1);
}

// "bla/bla/file.txt" --> "file"
void misc_basename_without_extension(const char *path, char *name, int sizeofname);

/*
Convert between sane utf-8 and fucking insane windows strings.
All strings are \0-terminated.
Returned strings are statically allocated.
Strings must not be insanely long.
*/
#ifdef _WIN32
const char *misc_windows_to_utf8(const wchar_t *winstr);
const wchar_t *misc_utf8_to_windows(const char *utf8);
#endif


#endif    // MISC_H
