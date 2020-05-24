#include "sphere.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stb_image.h>
#include <stb_image_resize.h>
#include "common.h"
#include "display.h"
#include "vecmat.h"

#define RADIUS 1.f

bool sphere_caminside(struct Sphere sph)
{
	return (vec3_lengthSQUARED(sph.center) <= RADIUS*RADIUS);
}

struct Plane sphere_visplane(struct Sphere sph)
{
	assert(!sphere_caminside(sph));

	/*
	From the side, the sphere being split by visplane looks like this:

         \ /
          \___
         //\  \
        /|  \  |
	cam/  \__\/
       ^^^^^^^\^^^^^^
               \
                \
	         visplane

	Let D denote the (smallest) distance between visplane and camera. Recall that
	the camera is at (0,0,0). With similar triangles, we get

		D = |sph.center| - RADIUS^2/|sph.center|.

	The equation of the plane is

		projection of (x,y,z) onto normal = D,

	where normal=-sph.center is a normal vector of the plane. By writing the
	projection with dot product, we get

		((x,y,z) dot normal) / |normal| = D.

	From here, we get

		(x,y,z) dot normal = |sph.center|^2 - RADIUS^2.
	*/
	return (struct Plane){
		.normal = vec3_neg(sph.center),
		.constant = vec3_lengthSQUARED(sph.center) - RADIUS*RADIUS,
	};
}

#define IS_TRANSPARENT(alpha) ((alpha) < 0x80)

static void average_color(const uint8_t *data, size_t npixels, uint8_t *rgb)
{
	// yes, rgb math is bad ikr
	unsigned long long rsum = 0, gsum = 0, bsum = 0;
	size_t count = 0;

	for (size_t i = 0; i < npixels; i += 4) {
		if (IS_TRANSPARENT(data[4*i + 3]))
			continue;

		rsum += data[4*i];
		gsum += data[4*i + 1];
		bsum += data[4*i + 2];
		count++;
	}

	if (count == 0) {
		// just set it to something, avoid divide by zero
		rgb[0] = 0xff;
		rgb[1] = 0xff;
		rgb[2] = 0xff;
	} else {
		rgb[0] = (uint8_t) iclamp((int)(rsum / count), 0, 0xff);
		rgb[1] = (uint8_t) iclamp((int)(gsum / count), 0, 0xff);
		rgb[2] = (uint8_t) iclamp((int)(bsum / count), 0, 0xff);
	}
}

static void replace_alpha_with_average(uint8_t *rgba, size_t npixels)
{
	uint8_t rgbavg[3];
	average_color(rgba, npixels, rgbavg);

	for (size_t i = 0; i < npixels; i += 1) {
		if (IS_TRANSPARENT(rgba[4*i + 3])) {
			rgba[4*i] = rgbavg[0];
			rgba[4*i + 1] = rgbavg[1];
			rgba[4*i + 2] = rgbavg[2];
		}
		rgba[4*i + 3] = 0xff;
	}
}

static SphereColorArray *read_image(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f)
		fatal_error("fopen", strerror(errno));

	int chansinfile, tmpw, tmph;
	uint8_t *readbuf = stbi_load_from_file(f, &tmpw, &tmph, &chansinfile, 4);
	fclose(f);
	if (!readbuf)
		fatal_error("stbi_load_from_file", strerror(errno));

	replace_alpha_with_average(readbuf, (size_t)tmpw*(size_t)tmph);

	uint8_t *reszbuf = malloc(SPHERE_PIXELS_AROUND * SPHERE_PIXELS_VERTICALLY * 4);
	if (!reszbuf)
		fatal_error("malloc", strerror(errno));

	int ok = stbir_resize_uint8(
		readbuf, tmpw, tmph, 0,
		reszbuf, SPHERE_PIXELS_AROUND, SPHERE_PIXELS_VERTICALLY, 0,
		4);
	free(readbuf);
	if (!ok)
		fatal_error("stbir_resize_uint8", strerror(errno));

	for (size_t i = 0; i < SPHERE_PIXELS_AROUND * SPHERE_PIXELS_VERTICALLY; i++)
	{
		SDL_Color col = { reszbuf[4*i], reszbuf[4*i+1], reszbuf[4*i+2], reszbuf[4*i+3] };
		static_assert(sizeof(col) == 4, "u got weird paddings");
		*(SDL_Color*) &reszbuf[4*i] = col;
	}

	return (void*)reszbuf;
}

struct Sphere sphere_load(const char *filename, struct Vec3 center)
{
	struct Sphere sph;
	sph.center = center;
	sph.colorarr = read_image(filename);
	return sph;
}

void sphere_destroy(struct Sphere sph)
{
	free(sph.colorarr);
}

// +1 because beginning and end are both needed
#define VECTORS_IDX(V, A) ((V)*SPHERE_PIXELS_AROUND + (A))
#define N_VECTORS VECTORS_IDX(SPHERE_PIXELS_VERTICALLY+1, 0)

/*
Where on the sphere's surface will each pixel go? This function calculates vectors
so that you don't need to call slow trig functions every time the sphere is drawn.
The resulting vectors have (0,0,0) as the sphere center, not as the camera.
*/
static const struct Vec3 *get_vectors(void)
{
	static struct Vec3 res[N_VECTORS];
	static bool ready = false;

	if (ready)
		return res;

	float pi = acosf(-1);

	for (size_t v = 0; v <= /* not < */ SPHERE_PIXELS_VERTICALLY; v++) {
		float y = 2*RADIUS*(float)v/SPHERE_PIXELS_VERTICALLY - RADIUS;
		float xzrad = sqrtf(RADIUS*RADIUS - y*y);  // radius on xz plane

		for (size_t a = 0; a < SPHERE_PIXELS_AROUND; a++) {
			float angle = (float)a/SPHERE_PIXELS_AROUND * 2*pi;
			float x = xzrad*sinf(angle);
			float z = -xzrad*cosf(angle);
			res[VECTORS_IDX(v, a)] = (struct Vec3){ x, y, z };
		}
	}

	ready = true;
	return res;
}

static struct Display4Gon get_4gon(
	struct Sphere sph, const struct Vec3 *vecs,
	size_t v, size_t v2, size_t a, size_t a2)
{
	return (struct Display4Gon){
		vec3_add(vecs[VECTORS_IDX(v, a)], sph.center),
		vec3_add(vecs[VECTORS_IDX(v, a2)], sph.center),
		vec3_add(vecs[VECTORS_IDX(v2, a)], sph.center),
		vec3_add(vecs[VECTORS_IDX(v2, a2)], sph.center),
	};
}

void sphere_display(struct Sphere sph, struct SDL_Renderer *rnd)
{
	if (sphere_caminside(sph))
		return;

	struct Plane vplane = sphere_visplane(sph);
	plane_move(&vplane, sph.center);

	// parts of image in front of this can be approximated with rectangles
	struct Plane rectplane = vplane;
	plane_move(&rectplane, vec3_withlength(rectplane.normal, 0.15f*RADIUS));

	const struct Vec3 *vecs = get_vectors();

	for (size_t a = 0; a < SPHERE_PIXELS_AROUND; a++) {
		size_t a2 = (a+1) % SPHERE_PIXELS_AROUND;

		for (size_t v = 0; v < SPHERE_PIXELS_VERTICALLY; v++) {
			size_t v2 = v+1;

			if (!plane_whichside(vplane, vecs[VECTORS_IDX(v, a)]) &&
				!plane_whichside(vplane, vecs[VECTORS_IDX(v, a2)]) &&
				!plane_whichside(vplane, vecs[VECTORS_IDX(v2, a)]) &&
				!plane_whichside(vplane, vecs[VECTORS_IDX(v2, a2)]))
			{
				continue;
			}

			SDL_Color col = (*sph.colorarr)[v][a];

			struct Display4Gon gon = get_4gon(sph, vecs, v,v2,a,a2);
			enum DisplayKind k =
				plane_whichside(rectplane, vecs[VECTORS_IDX(v, a)])
				? DISPLAY_RECT : DISPLAY_BORDER;
			display_4gon(rnd, gon, col, k);
		}
	}
}
