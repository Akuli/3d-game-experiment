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

	sph->angle = 0.f;
	sph->center = center;
	read_image(filename, (SDL_Color*) sph->image);
	return sph;
}

/*
A part of the sphere is visible to the camera. The rest isn't. The plane returned
by this function divides the sphere into the visible part and the part behind the
visible part. The normal vector of the plane points toward the visible side, so
plane_whichside() returns whether a point on the sphere is visible.

The returned plane is in camera coordinates.
*/
static struct Plane
get_visibility_plane(const struct Sphere *sph, const struct Camera *cam)
{
	assert(!sphere_contains(sph, cam->location));

	// switch to coordinates where camera is at (0,0,0)
	struct Vec3 center = camera_point_world2cam(cam, sph->center);

	/*
	From the side, the sphere being split by the visibility plane looks like this:

        \  /
         \/___
         /\   \
        /| \o  |
	   /  \_\_/
    cam^^^^^^\^^^^^^^
              \
               \
	       visibility
	         plane

	Note that the plane is closer to the camera than the sphere center. The center
	is marked with 'o' above.

	Let D denote the distance between visplane and camera. With similar triangles
	and Pythagorean theorem, we get

		D = |center| - RADIUS^2/|center|.

	As a quick sanity check, D < |center|. The equation of the plane is

		projection of (x,y,z) onto the center vector = D,

	because center is a normal vector of the plane. By writing the projection with
	dot product, we get

		((x,y,z) dot center) / |center| = D.

	We actually want the normal vector to point towards the camera, so we write the
	plane equation using -center instead of center:

		(x,y,z) dot (-center) = RADIUS^2 - |center|^2
	*/
	return (struct Plane) {
		.normal = vec3_neg(center),
		.constant = RADIUS*RADIUS - vec3_lengthSQUARED(center),
	};
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

	for (size_t v = 0; v < SPHERE_PIXELS_VERTICALLY+1; v++) {
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

void sphere_display(struct Sphere *sph, const struct Camera *cam)
{
	if (sphere_contains(sph, cam->location))
		return;

	/*
	sph->angle is in world coordinates. We want to do everything in camera
	coordinates. This matrix is the thing to use when it's tempting to rotate
	by sph->angle.
	*/
	struct Mat3 mat = mat3_mul_mat3(cam->world2cam, mat3_rotation_xz(sph->angle));

	struct Vec3 center = camera_point_world2cam(cam, sph->center);
	struct Plane vplane = get_visibility_plane(sph, cam);

	const VectorArray *rvecs = get_relative_vectors();
	for (size_t v = 0; v < SPHERE_PIXELS_VERTICALLY + 1; v++)
		for (size_t a = 0; a < SPHERE_PIXELS_AROUND; a++)
			sph->vectorcache[v][a] = vec3_add(center, mat3_mul_vec3(mat, (*rvecs)[v][a]));

	for (size_t a = 0; a < SPHERE_PIXELS_AROUND; a++) {
		size_t a2 = (a+1) % SPHERE_PIXELS_AROUND;

		for (size_t v = 0; v < SPHERE_PIXELS_VERTICALLY; v++) {
			size_t v2 = v+1;

			// this is perf critical code

			if (!plane_whichside(vplane, sph->vectorcache[v][a]) &&
				!plane_whichside(vplane, sph->vectorcache[v][a2]) &&
				!plane_whichside(vplane, sph->vectorcache[v2][a]) &&
				!plane_whichside(vplane, sph->vectorcache[v2][a2]))
			{
				continue;
			}

			struct Vec3 vecs[] = {
				sph->vectorcache[v][a],
				sph->vectorcache[v][a2],
				sph->vectorcache[v2][a],
				sph->vectorcache[v2][a2],
			};
			display_containing_rect(
				cam, sph->image[v][a], vecs, sizeof(vecs)/sizeof(vecs[0]));
		}
	}
}
