#include "sphere.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stb_image.h>
#include <stb_image_resize.h>
#include "camera.h"
#include "common.h"
#include "display.h"
#include "vecmat.h"

#define RADIUS 0.5f

bool sphere_contains(const struct Sphere *sph, struct Vec3 pt)
{
	return (vec3_lengthSQUARED(vec3_sub(sph->center, pt)) <= RADIUS*RADIUS);
}

struct Plane sphere_visplane(const struct Sphere *sph, const struct Camera *cam)
{
	assert(!sphere_contains(sph, cam->location));

	// switch to coordinates where camera is at (0,0,0)
	struct Vec3 center = camera_point_world2cam(cam, sph->center);

	/*
	From the side, the sphere being split by visplane looks like this:

         \ /
          \___
         //\  \
        /|  \  |
	   /  \__\/
    cam^^^^^^^\^^^^^^
               \
                \
	         visplane

	Let D denote the (smallest) distance between visplane and camera. Recall that
	the camera is at (0,0,0). With similar triangles, we get

		D = |sph.center| - RADIUS^2/|sph.center|.

	The equation of the plane is

		projection of (x,y,z) onto normal = -D,

	where normal=-sph.center is a normal vector of the plane pointing towards the
	camera. By writing the projection with dot product, we get

		((x,y,z) dot normal) / |normal| = -D.

	From here, we get

		(x,y,z) dot normal = RADIUS^2 - |sph.center|^2.
	*/
	struct Plane res = {
		.normal = vec3_neg(center),
		.constant = RADIUS*RADIUS - vec3_lengthSQUARED(center),
	};
	return camera_plane_cam2world(cam, res);
}

#define IS_TRANSPARENT(color) ((color).a < 0x80)

static SDL_Color average_color(SDL_Color *pixels, size_t npixels)
{
	// yes, rgb math is bad ikr
	unsigned long long rsum = 0, gsum = 0, bsum = 0;
	size_t count = 0;

	for (size_t i = 0; i < npixels; i += 4) {
		if (IS_TRANSPARENT(pixels[i]))
			continue;

		rsum += pixels[i].r;
		gsum += pixels[i].g;
		bsum += pixels[i].b;
		count++;
	}

	if (count == 0) {
		// just return something, avoid divide by zero
		return (SDL_Color){ 0xff, 0xff, 0xff, 0xff };
	}

	return (SDL_Color){
		(uint8_t) iclamp((int)(rsum / count), 0, 0xff),
		(uint8_t) iclamp((int)(gsum / count), 0, 0xff),
		(uint8_t) iclamp((int)(bsum / count), 0, 0xff),
		0xff,
	};
}

static void replace_alpha_with_average(SDL_Color *pixels, size_t npixels)
{
	SDL_Color avg = average_color(pixels, npixels);

	for (size_t i = 0; i < npixels; i += 1) {
		if (IS_TRANSPARENT(pixels[i]))
			pixels[i] = avg;
		pixels[i].a = 0xff;
	}
}

static void read_image(const char *filename, SDL_Color *res)
{
	FILE *f = fopen(filename, "rb");
	if (!f)
		fatal_error("fopen", strerror(errno));

	/*
	On a "typical" system, we can convert between SDL_Color arrays and uint8_t
	arrays with just casts.
	*/
	static_assert(sizeof(SDL_Color) == 4, "weird padding");
	static_assert(offsetof(SDL_Color, r) == 0, "weird structure order");
	static_assert(offsetof(SDL_Color, g) == 1, "weird structure order");
	static_assert(offsetof(SDL_Color, b) == 2, "weird structure order");
	static_assert(offsetof(SDL_Color, a) == 3, "weird structure order");

	int chansinfile, filew, fileh;
	SDL_Color *filedata = (SDL_Color*) stbi_load_from_file(f, &filew, &fileh, &chansinfile, 4);
	fclose(f);
	if (!filedata)
		fatal_error("stbi_load_from_file", strerror(errno));

	replace_alpha_with_average(filedata, (size_t)filew*(size_t)fileh);

	int ok = stbir_resize_uint8(
		(uint8_t*) filedata, filew, fileh, 0,
		(uint8_t*) res, SPHERE_PIXELS_AROUND, SPHERE_PIXELS_VERTICALLY, 0,
		4);
	free(filedata);
	if (!ok)
		fatal_error("stbir_resize_uint8", strerror(errno));
}

struct Sphere *sphere_load(const char *filename, struct Vec3 center)
{
	struct Sphere *sph = malloc(sizeof(*sph));
	if (!sph)
		fatal_error_nomem();

	sph->center = center;
	read_image(filename, (SDL_Color*) sph->image);
	return sph;
}

// +1 for vertical because we want to include both ends
// no +1 for the other one because it wraps around instead
typedef struct Vec3 VectorArray[SPHERE_PIXELS_VERTICALLY + 1][SPHERE_PIXELS_AROUND];

/*
Where on the sphere's surface will each pixel go? This function calculates vectors
so that you don't need to call slow trig functions every time the sphere is drawn.
The resulting vectors have (0,0,0) as the sphere center, hence "relative".
*/
static const VectorArray *get_relative_vectors(void)
{
	static VectorArray res;
	static bool ready = false;

	if (ready)
		return (const VectorArray *) &res;

	float pi = acosf(-1);

	for (size_t v = 0; v <= /* not < */ SPHERE_PIXELS_VERTICALLY; v++) {
		float y = RADIUS - 2*RADIUS*(float)v/SPHERE_PIXELS_VERTICALLY;
		float xzrad = sqrtf(RADIUS*RADIUS - y*y);  // radius on xz plane

		for (size_t a = 0; a < SPHERE_PIXELS_AROUND; a++) {
			float angle = (float)a/SPHERE_PIXELS_AROUND * 2*pi;
			float x = xzrad*sinf(angle);
			float z = xzrad*cosf(angle);
			res[v][a] = (struct Vec3){ x, y, z };
		}
	}

	ready = true;
	return (const VectorArray *) &res;
}

void sphere_display(const struct Sphere *sph, const struct Camera *cam)
{
	if (sphere_contains(sph, cam->location))
		return;

	struct Plane vplane = sphere_visplane(sph, cam);
	plane_move(&vplane, vec3_neg(sph->center));

	// parts of image in front of this can be approximated with rectangles
	struct Plane rectplane = vplane;
	plane_move(&rectplane, vec3_withlength(vplane.normal, 0.15f*RADIUS));

	const VectorArray *vecs = get_relative_vectors();

	for (size_t a = 0; a < SPHERE_PIXELS_AROUND; a++) {
		size_t a2 = (a+1) % SPHERE_PIXELS_AROUND;

		for (size_t v = 0; v < SPHERE_PIXELS_VERTICALLY; v++) {
			size_t v2 = v+1;

			if (!plane_whichside(vplane, (*vecs)[v][a]) &&
				!plane_whichside(vplane, (*vecs)[v][a2]) &&
				!plane_whichside(vplane, (*vecs)[v2][a]) &&
				!plane_whichside(vplane, (*vecs)[v2][a2]))
			{
				continue;
			}

			SDL_Color col = sph->image[v][a];
			struct Display4Gon gon = {
				vec3_add((*vecs)[v][a], sph->center),
				vec3_add((*vecs)[v][a2], sph->center),
				vec3_add((*vecs)[v2][a], sph->center),
				vec3_add((*vecs)[v2][a2], sph->center),
			};
			enum DisplayKind k = plane_whichside(rectplane, (*vecs)[v][a]) ? DISPLAY_RECT : DISPLAY_BORDER;

			display_4gon(cam, gon, col, k);
		}
	}
}
