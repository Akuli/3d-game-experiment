#ifndef MISC_H
#define MISC_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>

// windows.h defines min and max macros just like the ones below
#if !defined(min) && !defined(max)
	#define min(a,b) ((a)<(b) ? (a) : (b))
	#define max(a,b) ((a)>(b) ? (a) : (b))
#endif

inline void clamp(int *val, int lo, int hi) {
	if (*val < lo) *val = lo;
	if (*val > hi) *val = hi;
}
inline void clamp_float(float *val, float lo, float hi) {
	if (*val < lo) *val = lo;
	if (*val > hi) *val = hi;
}

enum State {
	STATE_CHOOSER,
	STATE_MAPEDITOR,
	STATE_DELETEMAP,
	STATE_PLAY,
	STATE_GAMEOVER,
	STATE_QUIT,
};

// logs errors, won't work with non-ascii paths on windows
void my_mkdir(const char *path);

/*
In this game, we don't want to distinguish some scancodes from each other to allow
multiple ways to do the same thing. For all different scan codes that should do the same
thing, this function returns the same scancode.
*/
int normalize_scancode(int sc);

/*
Blit src onto dst so that center of src goes to *center. If center is NULL, then
center of src goes to center of dst.
*/
void blit_with_center(SDL_Surface *src, SDL_Surface *dst, const SDL_Point *center);

// do not free the returned font
TTF_Font *get_font(int fontsz);

// Return a surface containing text on transparent background. Never returns NULL.
SDL_Surface *create_text_surface(const char *text, SDL_Color col, int fontsz);

// Use free_image_surface() only for surfaces returned from misc_create_surface()
SDL_Surface *create_image_surface(const char *path);
void free_image_surface(SDL_Surface *s);

/*
Create a surface that refers to another another surface. So, drawing to the
returned surface actually draws to the surface given as argument. This turns out
to be much faster than blitting. Never returns NULL.
*/
SDL_Surface *create_cropped_surface(SDL_Surface *surf, SDL_Rect r);

// 24 rightmost bits used, 8 bits for each of R,G,B
// very perf critical in wall drawing code, especially in maps with lots of walls
inline uint32_t rgb_average(uint32_t a, uint32_t b) {
	return ((a & 0xfefefe) >> 1) + ((b & 0xfefefe) >> 1);
}

// "bla/bla/file.txt" --> "file"
void basename_without_extension(const char *path, char *name, int sizeofname);

/*
Convert between sane utf-8 and fucking insane windows strings.
All strings are \0-terminated.
Returned strings are statically allocated.
Strings must not be insanely long.
*/
#ifdef _WIN32
const char *windows_to_utf8(const wchar_t *winstr);
const wchar_t *utf8_to_windows(const char *utf8);
#endif

// https://en.wikipedia.org/wiki/UTF-8#Encoding
#define is_utf8_continuation_byte(b) ((unsigned char)(b) >> 6 == 2)
void utf8_prev(char **s);  // Be careful with beginning of string
void utf8_next(char **s);
void utf8_prev_const(const char **s);  // Be careful with beginning of string
void utf8_next_const(const char **s);

#endif    // MISC_H
