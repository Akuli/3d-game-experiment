#ifndef MISC_H
#define MISC_H

#include <SDL2/SDL.h>

enum MiscState {
	MISC_STATE_CHOOSER,
	MISC_STATE_PLAY,
	MISC_STATE_GAMEOVER,
	MISC_STATE_QUIT,
};

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

// "bla/bla/file.txt" --> "file"
void misc_basename_without_extension(const char *path, char *name, int sizeofname);


#endif    // MISC_H
