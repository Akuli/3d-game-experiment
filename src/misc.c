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
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "log.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../stb/stb_image.h"


void misc_mkdir(const char *path)
{
#ifdef _WIN32
	int ret = _wmkdir(misc_utf8_to_windows(path));    // sets errno
#else
	// python uses 777 as default perms, see help(os.mkdir)
	// It's ANDed with current umask
	int ret = mkdir(path, 0777);
#endif

	if (ret != 0 && errno != EEXIST)
		log_printf_abort("creating directory '%s' failed: %s", path, strerror(errno));
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

// indexed by font size
TTF_Font *loaded_fonts[100] = {0};

static void close_loaded_fonts(void)
{
	for (int i = 0; i < sizeof(loaded_fonts)/sizeof(loaded_fonts[0]); i++) {
		if (loaded_fonts[i])
			TTF_CloseFont(loaded_fonts[i]);
	}
}

TTF_Font *misc_get_font(int fontsz)
{
	int n = sizeof(loaded_fonts)/sizeof(loaded_fonts[0]);
	SDL_assert(0 < fontsz && fontsz < n);
	if (loaded_fonts[fontsz])
		return loaded_fonts[fontsz];

	bool allnull = true;
	for (int i = 0; i < n; i++) {
		if (loaded_fonts[i])
			allnull = false;
	}
	if (allnull)
		atexit(close_loaded_fonts);

	loaded_fonts[fontsz] = TTF_OpenFont("assets/DejaVuSans.ttf", fontsz);
	if (!loaded_fonts[fontsz])
		log_printf_abort("TTF_OpenFont failed: %s", TTF_GetError());
	return loaded_fonts[fontsz];
}

SDL_Surface *misc_create_text_surface(const char *text, SDL_Color col, int fontsz)
{
	// It fails with zero length text, lol
	if (!*text)
		text = " ";

	SDL_Surface *s = TTF_RenderUTF8_Blended(misc_get_font(fontsz), text, col);
	if (!s)
		log_printf_abort("TTF_RenderUTF8_Blended failed: %s", TTF_GetError());
	return s;
}

SDL_Surface *misc_create_image_surface(const char *path)
{
	int fmt, w, h;
	unsigned char *data = stbi_load(path, &w, &h, &fmt, 4);
	if (!data)
		log_printf_abort("loading image from '%s' failed: %s", path, stbi_failure_reason());

	// SDL_CreateRGBSurfaceWithFormatFrom docs have example code for using it with stbi :D
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(
		data, w, h, 32, 4*w, SDL_PIXELFORMAT_RGBA32);
	if (!s)
		log_printf_abort("SDL_CreateRGBSurfaceWithFormatFrom failed: %s", SDL_GetError());
	SDL_assert(s->pixels == data);
	return s;
}

void misc_free_image_surface(SDL_Surface *s)
{
	stbi_image_free(s->pixels);
	SDL_FreeSurface(s);
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
