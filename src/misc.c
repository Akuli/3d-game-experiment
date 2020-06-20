#include "misc.h"
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

SDL_Surface *misc_create_text_surface(const char *text, SDL_Color col)
{
	// font not cached because this doesn't seem to be too slow
	TTF_Font *font = TTF_OpenFont("DejaVuSans.ttf", MISC_TEXT_SIZE);
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
	// TODO: use surf->format somehow?
	SDL_Surface *res = SDL_CreateRGBSurfaceFrom(
		(char*)surf->pixels + r.y*surf->pitch + sizeof(uint32_t)*r.x,
		r.w, r.h,
		32, surf->pitch, 0, 0, 0, 0);
	if (!res)
		log_printf_abort("SDL_CreateRGBSurfaceFrom failed: %s", SDL_GetError());
	return res;
}
