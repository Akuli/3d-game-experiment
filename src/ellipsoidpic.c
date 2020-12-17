#include "ellipsoid.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "../stb/stb_image.h"
#include "log.h"
#include "mathstuff.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define CLAMP_TO_U8(val) ( (unsigned char) min(max(val, 0), 0xff) )
#define IS_TRANSPARENT(alpha) ((alpha) < 0x80)

// yes, rgb math is bad ikr
static void replace_alpha_with_average(unsigned char *bytes, size_t npixels)
{
	long long rsum = 0, gsum = 0, bsum = 0;
	size_t count = 0;

	for (size_t i = 0; i < 4*npixels; i += 4) {
		if (!IS_TRANSPARENT(bytes[i+3])) {
			rsum += bytes[i];
			gsum += bytes[i+1];
			bsum += bytes[i+2];
			count++;
		}
	}

	if (count == 0)
		return;

	for (size_t i = 0; i < 4*npixels; i += 4) {
		if (IS_TRANSPARENT(bytes[i+3])) {
			bytes[i] = CLAMP_TO_U8(rsum / count);
			bytes[i+1] = CLAMP_TO_U8(gsum / count);
			bytes[i+2] = CLAMP_TO_U8(bsum / count);
		}
	}
}

static float linear_map(float srcmin, float srcmax, float dstmin, float dstmax, float val)
{
	float ratio = (val - srcmin)/(srcmax - srcmin);
	return dstmin + ratio*(dstmax - dstmin);
}

// returns between 0 and 2pi
static float calculate_angle(int x, int z)
{
	float pi = acosf(-1);

	/*
	This angle is chosen so that angle=0 means toward positive z axis and player
	pictures don't show up mirrored. That makes the player stuff easier because
	the front of the player at angle=pi should point in negative z direction.
	*/
	float res = pi/2 - atan2f(z - ELLIPSOIDPIC_SIDE/2, x - ELLIPSOIDPIC_SIDE/2);
	if (res < 2*pi) res += 2*pi;
	if (res > 2*pi) res -= 2*pi;
	SDL_assert(0 <= res && res <= 2*pi);
	return res;
}

// the atan2 in calculate_angle() is slow, let's cache it
typedef float AngleArray[ELLIPSOIDPIC_SIDE][ELLIPSOIDPIC_SIDE];
static const AngleArray *get_angle_array(void)
{
	static AngleArray res;
	static bool loaded = false;

	if (!loaded) {
		for (int x = 0; x < ELLIPSOIDPIC_SIDE; x++) {
			for (int z = 0; z < ELLIPSOIDPIC_SIDE; z++) {
				res[x][z] = calculate_angle(x, z);
			}
		}
		loaded = true;
	}
	return (const AngleArray *) &res;
}

/*
Currently there's no way to give a utf-8 filename to stbi_load(), unless you
use Microsoft's compiler. There's also a buffer overflow.
https://github.com/nothings/stb/issues/939
*/
static FILE *open_binary_file_for_reading(const char *path)
{
#ifdef _WIN32
	wchar_t wpath[1024];
	int n = MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, sizeof(wpath)/sizeof(wpath[0]) - 1);
	if (n == 0)
		log_printf_abort("MultiByteToWideChar with utf8 string '%s' failed", path);

	SDL_assert(0 < n && n < sizeof(wpath)/sizeof(wpath[0]));
	wpath[n] = L'\0';
	FILE *f = _wfopen(wpath, L"rb");
#else
	FILE *f = fopen(path, "rb");
#endif

	if (!f)
		log_printf_abort("opening '%s' failed: %s", path, strerror(errno));
	return f;
}

static void read_image(struct EllipsoidPic *epic)
{
	const AngleArray *angles = get_angle_array();

	FILE *f = open_binary_file_for_reading(epic->path);
	int chansinfile, filew, fileh;
	unsigned char *filedata = stbi_load_from_file(f, &filew, &fileh, &chansinfile, 4);
	fclose(f);
	if (!filedata)
		log_printf_abort("stbi_load failed for file opened from '%s': %s", epic->path, stbi_failure_reason());

	replace_alpha_with_average(filedata, (size_t)filew*(size_t)fileh);

	float pi = acosf(-1);

	// triple for loop without much indentation (lol)
	for (int x = 0; x < ELLIPSOIDPIC_SIDE; x++)
	for (int y = 0; y < ELLIPSOIDPIC_SIDE; y++)
	for (int z = 0; z < ELLIPSOIDPIC_SIDE; z++)
	{
		int picy = (int)linear_map(ELLIPSOIDPIC_SIDE-1, 0, 0, (float)(fileh-1), (float)y);
		SDL_assert(0 <= picy && picy < fileh);

		float angle = (*angles)[x][z];
		int picx = (int)linear_map(0, 2*pi, 0, (float)(filew-1), angle);
		SDL_assert(0 <= picx && picx < filew);

		size_t i = (size_t)( (picy*filew + picx)*4 );
		epic->cubepixels[x][y][z] = SDL_MapRGB(
			epic->pixfmt, filedata[i], filedata[i+1], filedata[i+2]);
	}

	stbi_image_free(filedata);
}

void ellipsoidpic_load(
	struct EllipsoidPic *epic, const char *path, const SDL_PixelFormat *fmt)
{
	log_printf("Loading ellipsoid pic: %s\n", path);
	snprintf(epic->path, sizeof(epic->path), "%s", path);
	epic->pixfmt = fmt;
	read_image(epic);
	epic->hidelowerhalf = false;
}
