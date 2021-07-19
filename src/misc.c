#ifdef _WIN32
	#include <direct.h>
	#include <windows.h>
	#include <wchar.h>
#else
	// includes from mkdir(2)
	#include <sys/stat.h>
	#include <sys/types.h>  // IWYU pragma: keep
#endif

#include "misc.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "log.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>


void misc_mkdir(const char *path)
{
#ifdef _WIN32
	int ret = _mkdir(path);
#else
	// python uses 777 as default perms, see help(os.mkdir)
	// It's ANDed with current umask
	int ret = mkdir(path, 0777);
#endif

	// TODO: better windows error handling than errno
	if (ret != 0)
		log_printf("mkdir(\"%s\") failed: %s", path, strerror(errno));
}

int misc_handle_scancode(int sc)
{
	switch(sc) {
		// numpad 0 --> usual 0
		case SDL_SCANCODE_KP_0:
			return SDL_SCANCODE_0;

		// numpad arrow keys --> actual arrow keys
		case SDL_SCANCODE_KP_4:
			return SDL_SCANCODE_LEFT;
		case SDL_SCANCODE_KP_6:
			return SDL_SCANCODE_RIGHT;
		case SDL_SCANCODE_KP_8:
			return SDL_SCANCODE_UP;
		case SDL_SCANCODE_KP_5:
		case SDL_SCANCODE_KP_2:
			return SDL_SCANCODE_DOWN;

		default:
			return sc;
	}
}

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
	TTF_Font *font = TTF_OpenFont("assets/DejaVuSans.ttf", fontsz);
	if (!font)
		log_printf_abort("TTF_OpenFont failed: %s", TTF_GetError());

	SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, col);
	TTF_CloseFont(font);
	if (!s)
		log_printf_abort("TTF_RenderUTF8_Blended failed: %s", TTF_GetError());
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

extern inline uint32_t misc_rgb_average(uint32_t a, uint32_t b);

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

// TODO: most of my code uses strerror(errno) for windows errors which is wrong?

#ifdef _WIN32
const char *misc_windows_to_utf8(const wchar_t *winstr)
{
	static char utf8[1024];
	int n = WideCharToMultiByte(
		CP_UTF8, 0,
		winstr, -1,
		utf8, sizeof(utf8) - 1,
		NULL, NULL);

	// if it fails, then we return empty string... which is somewhat sane-ish?
	// can't call log_printf because it wants utf8 string
	SDL_assert(0 <= n && n < sizeof(utf8));
	utf8[n] = 0;
	return utf8;
}

const wchar_t *misc_utf8_to_windows(const char *utf8)
{
	static wchar_t winstr[1024];
	int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, winstr, sizeof(winstr)/sizeof(winstr[0]) - 1);
	if (n == 0)
		log_printf_abort("MultiByteToWideChar with utf8 string '%s' failed", utf8);

	SDL_assert(0 < n && n < sizeof(winstr)/sizeof(winstr[0]));
	winstr[n] = L'\0';
	return winstr;
}
#endif
