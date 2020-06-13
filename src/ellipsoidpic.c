#include "ellipsoidpic.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "../stb/stb_image.h"
#include "log.h"
#include "mathstuff.h"

#define CLAMP_TO_U8(val) ( (unsigned char) min(max(val, 0), 0xff) )
#define IS_TRANSPARENT(alpha) ((alpha) < 0x80)

// yes, rgb math is bad ikr
static void replace_alpha_with_average(unsigned char *bytes, size_t nbytes)
{
	unsigned long long rsum = 0, gsum = 0, bsum = 0;
	size_t count = 0;

	for (size_t i = 0; i < 4*nbytes; i += 4) {
		if (!IS_TRANSPARENT(bytes[i+3])) {
			rsum += bytes[i];
			gsum += bytes[i+1];
			bsum += bytes[i+2];
			count++;
		}
	}

	if (count == 0)
		return;

	for (size_t i = 0; i < 4*nbytes; i += 4) {
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
static float get_angle(float x, float z)
{
	float pi = acosf(-1);

	/*
	This angle is chosen so that angle=0 means toward positive z axis and player
	pictures don't show up mirrored. That makes the player stuff easier because
	the front of the player at angle=pi should point in negative z direction.
	*/
	float res = pi/2 - atan2f(z, x);
	if (res < 2*pi) res += 2*pi;
	if (res > 2*pi) res -= 2*pi;
	return res;
}

static void read_image(const char *filename, struct EllipsoidPic *epic)
{
	FILE *f = fopen(filename, "rb");
	if (!f)
		log_printf_abort("opening '%s' failed: %s", filename, strerror(errno));

	int chansinfile, filew, fileh;
	unsigned char *filedata = stbi_load_from_file(f, &filew, &fileh, &chansinfile, 4);
	fclose(f);
	if (!filedata)
		log_printf_abort("stbi_load_from_file failed: %s", stbi_failure_reason());

	replace_alpha_with_average(filedata, (size_t)filew*(size_t)fileh);

	float pi = acosf(-1);

	// triple for loop without much indentation (lol)
	for (int x = 0; x < ELLIPSOIDPIC_SIDE; x++)
	for (int y = 0; y < ELLIPSOIDPIC_SIDE; y++)
	for (int z = 0; z < ELLIPSOIDPIC_SIDE; z++)
	{
		int picy = (int)linear_map(ELLIPSOIDPIC_SIDE-1, 0, 0, (float)(fileh-1), (float)y);
		assert(0 <= picy && picy < fileh);

		float angle = get_angle((float)x - ELLIPSOIDPIC_SIDE/2.f, (float)z - ELLIPSOIDPIC_SIDE/2.f);
		int picx = (int)linear_map(0, 2*pi, 0, (float)(filew-1), angle);
		assert(0 <= picx && picx < filew);

		size_t i = (size_t)( (picy*filew + picx)*4 );
		epic->cubepixels[x][y][z] = SDL_MapRGB(
			epic->pixfmt, filedata[i], filedata[i+1], filedata[i+2]);
	}
}

void ellipsoidpic_load(
	struct EllipsoidPic *epic, const char *filename, const SDL_PixelFormat *fmt)
{
	epic->pixfmt = fmt;
	read_image(filename, epic);
	epic->hidelowerhalf = false;
}
