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

static SphereColorArray *read_image(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	if (!f)
		fatal_error("fopen", strerror(errno));

	int chansinfile, tmpw, tmph;
	uint8_t *readbuf = stbi_load_from_file(f, &tmpw, &tmph, &chansinfile, 3);
	fclose(f);
	if (!readbuf)
		fatal_error("stbi_load_from_file", strerror(errno));

	uint8_t *reszbuf = malloc(SPHERE_PIXELS_AROUND * SPHERE_PIXELS_VERTICALLY * 3);
	if (!reszbuf)
		fatal_error("malloc", strerror(errno));

	int ok = stbir_resize_uint8(
		readbuf, tmpw, tmph, 0,
		reszbuf, SPHERE_PIXELS_AROUND, SPHERE_PIXELS_VERTICALLY, 0,
		3);
	if (!ok)
		fatal_error("stbir_resize_uint8", strerror(errno));

	SphereColorArray *colorarr = malloc(sizeof(*colorarr));
	if (!colorarr)
		fatal_error("malloc", strerror(errno));

	size_t i = 0;
	for (size_t y = 0; y < SPHERE_PIXELS_VERTICALLY; y++) {
		for (size_t ar = 0; ar < SPHERE_PIXELS_AROUND; ar++) {
			(*colorarr)[y][ar].r = reszbuf[i++];
			(*colorarr)[y][ar].g = reszbuf[i++];
			(*colorarr)[y][ar].b = reszbuf[i++];
		}
	}

	free(reszbuf);
	return colorarr;
}

static SDL_Color average_color(const SDL_Color *pixels, size_t npixels)
{
	// yes, rgb math is bad ikr
	unsigned long long rsum = 0, gsum = 0, bsum = 0;
	for (size_t i = 0; i < npixels; i++) {
		rsum += pixels[i].r;
		gsum += pixels[i].g;
		bsum += pixels[i].b;
	}

	return (SDL_Color){
		.r = (uint8_t) iclamp((int)(rsum / npixels), 0, 0xff),
		.g = (uint8_t) iclamp((int)(gsum / npixels), 0, 0xff),
		.b = (uint8_t) iclamp((int)(bsum / npixels), 0, 0xff),
		.a = 0xff,
	};
}

struct Sphere sphere_load(const char *filename, struct Vec3 center)
{
	struct Sphere sph;
	sph.center = center;
	sph.colorarr = read_image(filename);
	sph.bgcolor = average_color((SDL_Color*)sph.colorarr, (size_t)SPHERE_PIXELS_AROUND * (size_t)SPHERE_PIXELS_VERTICALLY);
	return sph;
}

void sphere_destroy(struct Sphere sph)
{
	free(sph.colorarr);
}

// 2* because half of the sphere is background color
// +1 because beginning and end are both needed
#define VECTORS_IDX(V, A) ((V)*( 2*SPHERE_PIXELS_AROUND ) + (A))
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

		for (size_t a = 0; a < 2*SPHERE_PIXELS_AROUND; a++) {
			float angle = (float)a/SPHERE_PIXELS_AROUND * pi;
			float x = -xzrad*cosf(angle);
			float z = xzrad*sinf(angle);
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

	const struct Vec3 *vecs = get_vectors();

	for (size_t a = 0; a < 2*SPHERE_PIXELS_AROUND; a++) {
		size_t a2 = (a+1) % (2*SPHERE_PIXELS_AROUND);

		for (size_t v = 0; v < SPHERE_PIXELS_VERTICALLY; v++) {
			size_t v2 = v+1;

			if (!plane_whichside(vplane, vecs[VECTORS_IDX(v, a)]) ||
				!plane_whichside(vplane, vecs[VECTORS_IDX(v, a2)]) ||
				!plane_whichside(vplane, vecs[VECTORS_IDX(v2, a)]) ||
				!plane_whichside(vplane, vecs[VECTORS_IDX(v2, a2)]))
			{
				continue;
			}

			SDL_Color col = (a < SPHERE_PIXELS_AROUND) ? (*sph.colorarr)[v][a] : sph.bgcolor;
			SDL_SetRenderDrawColor(rnd, col.r, col.g, col.b, col.a);
			display_4gon(rnd, get_4gon(sph, vecs, v,v2,a,a2));
		}
	}
}
