#include "ellipsoid.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stb_image.h>
#include <stb_image_resize.h>
#include "camera.h"
#include "log.h"
#include "ellipsemove.h"
#include "mathstuff.h"

#define CLAMP_TO_U8(val) ( (uint8_t) min(max(val, 0), 0xff) )
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
		CLAMP_TO_U8(rsum / count),
		CLAMP_TO_U8(gsum / count),
		CLAMP_TO_U8(bsum / count),
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
		log_printf_abort("opening '%s' failed: %s", filename, strerror(errno));

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
		log_printf_abort("stbi_load_from_file failed: %s", stbi_failure_reason());

	replace_alpha_with_average(filedata, (size_t)filew*(size_t)fileh);

	int ok = stbir_resize_uint8(
		// afaik unsigned char works better with strict aliasing rules than uint8_t
		(unsigned char *) filedata, filew, fileh, 0,
		(unsigned char *) res, ELLIPSOID_PIXELS_AROUND, ELLIPSOID_PIXELS_VERTICALLY, 0,
		4);
	stbi_image_free(filedata);
	if (!ok)
		log_printf_abort("stbir_resize_uint8 failed: %s", stbi_failure_reason());
}

void ellipsoid_load(struct Ellipsoid *el, const char *filename)
{
	memset(el, 0, sizeof(*el));
	read_image(filename, (SDL_Color*) el->image);

	el->angle = 0;
	el->xzradius = 1;
	el->yradius = 1;
	ellipsoid_update_transforms(el);
}

/*
We introduce third type of coordinates: unit ball coordinates
- ellipsoid center is at (0,0,0)
- ellipsoid radius is 1
- cam->world2cam transform hasn't been applied yet
*/

// +1 for vertical because we want to include both ends
// no +1 for the other one because it wraps around instead
typedef Vec3 VectorArray[ELLIPSOID_PIXELS_VERTICALLY + 1][ELLIPSOID_PIXELS_AROUND];

/*
Where on the ellipsoid's surface will each pixel go? This function calculates vectors
so that we don't need to call slow trig functions every time an ellipsoid is drawn.
Returns unit ball coordinates.
*/
static const VectorArray *get_untransformed_surface_vectors(void)
{
	static VectorArray res;
	static bool ready = false;

	if (ready)
		return (const VectorArray *) &res;

	float pi = acosf(-1);

	for (size_t v = 0; v < ELLIPSOID_PIXELS_VERTICALLY+1; v++) {
		float y = 1 - 2*(float)v/ELLIPSOID_PIXELS_VERTICALLY;
		float xzrad = sqrtf(1 - y*y);  // radius on xz plane

		for (size_t a = 0; a < ELLIPSOID_PIXELS_AROUND; a++) {
			float angle = (float)a/ELLIPSOID_PIXELS_AROUND * 2*pi;
			float x = xzrad*sinf(angle);
			float z = xzrad*cosf(angle);
			res[v][a] = (Vec3){ x, y, z };
		}
	}

	ready = true;
	return (const VectorArray *) &res;
}

/*
A part of the ellipsoid is visible to the camera. The rest isn't. The plane returned
by this function divides the ellipsoid into the visible part and the part behind the
visible part. The normal vector of the plane points toward the visible side, so
plane_whichside() returns whether a point on the ellipsoid is visible.

The returned plane is in unit ball coordinates.
*/
static struct Plane
get_visibility_plane(const struct Ellipsoid *el, const struct Camera *cam)
{
	/*
	Calculate camera location in unit ball coordinates. This must work
	so that once the resulting camera vector is
		1. transformed with el->transform
		2. transformed with cam->world2cam
		3. added with el->center
	then we get the camera location in camera coordinates, i.e. (0,0,0).
	*/
	Vec3 cam2center = camera_point_world2cam(cam, el->center);
	Vec3 center2cam = vec3_neg(cam2center);
	vec3_apply_matrix(&center2cam, mat3_inverse(cam->world2cam));
	vec3_apply_matrix(&center2cam, el->transform_inverse);

	/*
	From the side, the el being split by the visibility plane looks like this:

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

	Note that the plane is closer to the camera than the el center. The center
	is marked with o above. Note that we are using unit ball coordinates,
	so we have o=(0,0,0).

	Let D denote the distance between visibility plane and the el center. With
	similar triangles and Pythagorean theorem, we get

		D = 1/|center2cam|,

	where 1 = 1^2 = (unit ball radius)^2. The equation of the plane is

		projection of (x,y,z) onto center2cam = D,

	because center2cam is a normal vector of the plane. By writing the projection
	with dot product, we get

		((x,y,z) dot center2cam) / |center2cam| = D.

	This simplifies:

		(x,y,z) dot center2cam = 1
	*/
	return (struct Plane) {
		.normal = center2cam,
		.constant = 1,
	};
}

#define convert_color(SURF, COL) \
	SDL_MapRGBA((SURF)->format, (COL).r, (COL).g, (COL).b, (COL.a))

void ellipsoid_show(const struct Ellipsoid *el, const struct Camera *cam)
{
	/*
	static to keep stack usage down, also turns out to be quite a bit faster than
	placing these caches into el. I also tried putting these to stack and that was
	about same speed as static.
	*/
	static Vec3 vectorcache[ELLIPSOID_PIXELS_VERTICALLY + 1][ELLIPSOID_PIXELS_AROUND];
	static bool sidecache[ELLIPSOID_PIXELS_VERTICALLY + 1][ELLIPSOID_PIXELS_AROUND];

	// vplane is in unit ball coordinates
	struct Plane vplane = get_visibility_plane(el, cam);

	// center vector as camera coordinates
	Vec3 center = camera_point_world2cam(cam, el->center);

	/*
	To convert from unit ball coordinates to camera coordinates, apply
	this and then add center
	*/
	Mat3 unitball2cam = mat3_mul_mat3(cam->world2cam, el->transform);

	const VectorArray *usvecs = get_untransformed_surface_vectors();
	for (size_t v = 0; v < ELLIPSOID_PIXELS_VERTICALLY + 1; v++) {
		for (size_t a = 0; a < ELLIPSOID_PIXELS_AROUND; a++) {
			// this is perf critical code

			sidecache[v][a] = plane_whichside(vplane, (*usvecs)[v][a]);

			// turns out that in-place operations are measurably faster
			vectorcache[v][a] = (*usvecs)[v][a];
			vec3_apply_matrix(&vectorcache[v][a], unitball2cam);
			vec3_add_inplace(&vectorcache[v][a], center);
		}
	}

	size_t vmax = ELLIPSOID_PIXELS_VERTICALLY;
	if (el->hidelowerhalf)
		vmax /= 2;

	for (size_t a = 0; a < ELLIPSOID_PIXELS_AROUND; a++) {
		size_t a2 = (a+1) % ELLIPSOID_PIXELS_AROUND;

		for (size_t v = 0; v < vmax; v++) {
			size_t v2 = v+1;

			// this is perf critical code

			if (!sidecache[v][a] &&
				!sidecache[v][a2] &&
				!sidecache[v2][a] &&
				!sidecache[v2][a2])
			{
				continue;
			}

			SDL_Rect rect;
			if (camera_get_containing_rect(
					cam, &rect,
					vectorcache[v][a],
					vectorcache[v][a2],
					vectorcache[v2][a],
					vectorcache[v2][a2]))
			{
				SDL_FillRect(
					cam->surface, &rect,
					convert_color(cam->surface, el->image[v][a]));
			}
		}
	}
}

static Mat3 diag(float a, float b, float c)
{
	return (Mat3){ .rows = {
		{ a, 0, 0 },
		{ 0, b, 0 },
		{ 0, 0, c },
	}};
}

void ellipsoid_update_transforms(struct Ellipsoid *el)
{
	el->transform = mat3_mul_mat3(
		diag(el->xzradius, el->yradius, el->xzradius),
		mat3_rotation_xz(el->angle));
	el->transform_inverse = mat3_inverse(el->transform);
}

// Return rotation matrix that rotates given vector to have no z coordinate
static Mat3 z_canceling_rotation(Vec3 v, float len)
{
	// TODO: make rotation matrix rotate in different direction to get rid of minus sign here
	return mat3_inverse(mat3_rotation_xz_sincos(-v.z/len, v.x/len));
}

float ellipsoid_bump_amount(const struct Ellipsoid *el1, const struct Ellipsoid *el2)
{
	Vec3 diff = vec3_sub(el1->center, el2->center);
	float difflen = hypotf(diff.x, diff.z);   // ignore diff.y
	if (difflen < 1e-5f) {
		// ellipses very near each other
		return el1->xzradius + el2->xzradius;
	}

	// Rotate centers so that ellipsoid centers have same z coordinate
	Mat3 rot = z_canceling_rotation(diff, difflen);
	Vec3 center1 = mat3_mul_vec3(rot, el1->center);
	Vec3 center2 = mat3_mul_vec3(rot, el2->center);
	assert(fabsf(center1.z - center2.z) < 1e-5f);

	// Now this is a 2D problem on the xy plane (or some other plane parallel to xy plane)

	Vec2 center1_xy = { center1.x, center1.y };
	Vec2 center2_xy = { center2.x, center2.y };
	return ellipse_move_amount_x(
		el1->xzradius, el1->yradius, center1_xy,
		el2->xzradius, el2->yradius, center2_xy);
}

void ellipsoid_move_apart(struct Ellipsoid *el1, struct Ellipsoid *el2, float mv)
{
	assert(mv >= 0);
	Vec3 from1to2 = vec3_withlength(vec3_sub(el2->center, el1->center), mv/2);
	vec3_add_inplace(&el2->center, from1to2);
	vec3_add_inplace(&el1->center, vec3_neg(from1to2));
}
