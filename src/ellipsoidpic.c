#include "ellipsoidpic.h"
#include <errno.h>
#include "../stb/stb_image.h"
#include "../stb/stb_image_resize.h"
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

static void read_image(const char *filename, uint32_t *res, const SDL_PixelFormat *fmt)
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

	int ok = stbir_resize_uint8(
		filedata, filew, fileh, 0,
		(unsigned char *) res, ELLIPSOIDPIC_AROUND, ELLIPSOIDPIC_HEIGHT, 0,
		4);
	stbi_image_free(filedata);
	if (!ok)
		log_printf_abort("stbir_resize_uint8 failed: %s", stbi_failure_reason());

	for (size_t i = 0; i < ELLIPSOIDPIC_AROUND*ELLIPSOIDPIC_HEIGHT; i++) {
		// fingers crossed, hoping that i understood strict aliasing correctly...
		// https://stackoverflow.com/a/29676395
		unsigned char *ptr = (unsigned char *)&res[i];
		res[i] = SDL_MapRGBA(fmt, ptr[0], ptr[1], ptr[2], 0xff);
	}
}

void ellipsoidpic_load(
	struct EllipsoidPic *epic, const char *filename, const SDL_PixelFormat *fmt)
{
	epic->pixfmt = fmt;
	read_image(filename, (uint32_t *)epic->pixels, fmt);
	epic->hidelowerhalf = false;
}
