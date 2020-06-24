#include "misc.h"
#include <string.h>
#include "log.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void misc_blit_with_center(SDL_Surface *src, SDL_Surface *dst, const SDL_Point *center)
{
	int cx, cy;
	if (center) {
		cx = center->x;
		cy = center->y;
	} else {
		cx = dst->w/2;
		cy = dst->h/2;
	}
	SDL_Rect r = { cx - src->w/2, cy - src->h/2, src->w, src->h };
	SDL_BlitSurface(src, NULL, dst, &r);
}

SDL_Surface *misc_create_text_surface(const char *text, SDL_Color col, int fontsz)
{
	// font not cached because this isn't called in perf critical loops
	TTF_Font *font = TTF_OpenFont("DejaVuSans.ttf", fontsz);
	if (!font)
		log_printf_abort("TTF_OpenFont failed: %s", TTF_GetError());

	SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, col);
	TTF_CloseFont(font);
	if (!s)
		log_printf_abort("TTF_RenderUTF8_Solid failed: %s", TTF_GetError());
	return s;
}

SDL_Surface *misc_create_cropped_surface(SDL_Surface *surf, SDL_Rect r)
{
	SDL_assert(surf->format->BitsPerPixel == 8*surf->format->BytesPerPixel);
	SDL_Surface *res = SDL_CreateRGBSurfaceFrom(
		(char*)surf->pixels + r.y*surf->pitch + surf->format->BytesPerPixel*r.x,
		r.w, r.h,
		surf->format->BitsPerPixel,
		surf->pitch,
		surf->format->Rmask, surf->format->Gmask, surf->format->Bmask, surf->format->Amask);
	if (!res)
		log_printf_abort("SDL_CreateRGBSurfaceFrom failed: %s", SDL_GetError());
	return res;
}

void misc_basename_without_extension(const char *path, char *name, int sizeofname)
{
	if (strrchr(path, '/'))
		path = strrchr(path, '/') + 1;
	if (strrchr(path, '\\'))
		path = strrchr(path, '\\') + 1;

	const char *end = strchr(path, '.');
	if (!end)
		end = path + strlen(path);

	snprintf(name, sizeofname, "%.*s", (int)(end - path), path);
}
