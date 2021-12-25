#include "ellipsoid.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "../stb/stb_image.h"
#include "log.h"
#include "misc.h"
#include "glob.h"

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
			bytes[i] = (unsigned char)(rsum / count);
			bytes[i+1] = (unsigned char)(gsum / count);
			bytes[i+2] = (unsigned char)(bsum / count);
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

void ellipsoidpic_load(
	struct EllipsoidPic *epic, const char *path, const SDL_PixelFormat *fmt)
{
	log_printf("Loading ellipsoid pic: %s\n", path);
	snprintf(epic->path, sizeof(epic->path), "%s", path);
	epic->pixfmt = fmt;

	const AngleArray *angles = get_angle_array();

	int chansinfile, filew, fileh;
	unsigned char *filedata = stbi_load(epic->path, &filew, &fileh, &chansinfile, 4);
	if (!filedata)
		log_printf_abort("stbi_load failed with path '%s': %s", epic->path, stbi_failure_reason());

	replace_alpha_with_average(filedata, (size_t)filew*(size_t)fileh);

	float pi = acosf(-1);
	uint32_t red = epic->pixfmt->Rmask;

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
		epic->cubepixels[false][x][y][z] = SDL_MapRGB(
			epic->pixfmt, filedata[i], filedata[i+1], filedata[i+2]);
		epic->cubepixels[true][x][y][z] = rgb_average(epic->cubepixels[false][x][y][z], red);
	}

	stbi_image_free(filedata);
}

// no way to pass data to atexit callbacks
static struct {
	struct EllipsoidPic **arr;
	int len;
} epicarrays[10];
static int nepicarrays = 0;

static void atexit_callback(void)
{
	for (int k = 0; k < nepicarrays; k++) {
		for (int i = 0; i < epicarrays[k].len; i++)
			free(epicarrays[k].arr[i]);
		free(epicarrays[k].arr);
	}
}

struct EllipsoidPic *const *ellipsoidpic_loadmany(
	int *n, const char *globpat, const SDL_PixelFormat *fmt,
	void (*progresscb)(void *cbdata, int i, int n), void *cbdata)
{
	glob_t gl;
	if (glob(globpat, 0, NULL, &gl) != 0)
		log_printf_abort("globbing with \"%s\" failed", globpat);

	*n = (int)gl.gl_pathc;
	struct EllipsoidPic **epics = malloc(sizeof(epics[0]) * (*n));
	if (!epics)
		log_printf_abort("not enough memory for array of %d pointers", *n);

	SDL_assert(nepicarrays < sizeof(epicarrays)/sizeof(epicarrays[0]));
	epicarrays[nepicarrays].arr = epics;
	epicarrays[nepicarrays].len = *n;
	if (nepicarrays == 0)
		atexit(atexit_callback);
	nepicarrays++;

	for (int i = 0; i < *n; i++) {
		if (progresscb)
			progresscb(cbdata, i, *n);

		if (!( epics[i] = malloc(sizeof(*epics[0])) ))
			log_printf("not enough mem to load ellipsoid pic from \"%s\"", gl.gl_pathv[i]);
		ellipsoidpic_load(epics[i], gl.gl_pathv[i], fmt);
	}

	globfree(&gl);
	return epics;
}
