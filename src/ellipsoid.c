#include "ellipsoid.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stb_image.h>
#include <stb_image_resize.h>
#include "camera.h"
#include "log.h"
#include "ellipsemove.h"
#include "mathstuff.h"

// This file uses unsigned char instead of uint8_t because it works better with strict aliasing

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
		(unsigned char *) res, ELLIPSOID_PIXELS_AROUND, ELLIPSOID_PIXELS_VERTICALLY, 0,
		4);
	stbi_image_free(filedata);
	if (!ok)
		log_printf_abort("stbir_resize_uint8 failed: %s", stbi_failure_reason());

	for (size_t i = 0; i < ELLIPSOID_PIXELS_AROUND*ELLIPSOID_PIXELS_VERTICALLY; i++) {
		// fingers crossed, hoping that i understood strict aliasing correctly...
		// https://stackoverflow.com/a/29676395
		unsigned char *ptr = (unsigned char *)&res[i];
		res[i] = SDL_MapRGBA(fmt, ptr[0], ptr[1], ptr[2], 0xff);
	}
}

void ellipsoid_load(struct Ellipsoid *el, const char *filename, const SDL_PixelFormat *fmt)
{
	memset(el, 0, sizeof(*el));

	read_image(filename, (uint32_t *)el->image, fmt);
	el->pixfmt = fmt;

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
			/*
			+pi sets the angle of the back of the player, corresponding to a=0.
			This way, the player looks into the angle=0 direction.
			Minus sign is needed to avoid mirror imaging the pic for some reason.
			*/
			float angle = -(float)a/ELLIPSOID_PIXELS_AROUND * 2*pi + pi;
			float x = xzrad*cosf(angle);
			float z = xzrad*sinf(angle);
			res[v][a] = (Vec3){ x, y, z };
		}
	}

	ready = true;
	return (const VectorArray *) &res;
}

/*
A part of the ellipsoid is visible to the camera. The rest isn't. The plane returned
by this function splits the ellipsoid into the visible part and the part behind the
visible part. The normal vector of the plane points toward the visible side, so
plane_whichside() returns whether a point on the ellipsoid is visible.

The returned plane is in unit ball coordinates.
*/
static struct Plane
get_splitter_plane(const struct Ellipsoid *el, const struct Camera *cam)
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

void ellipsoid_show(const struct Ellipsoid *el, const struct Camera *cam)
{
	assert(cam->surface->format == el->pixfmt);

	/*
	static to keep stack usage down, also turns out to be quite a bit faster than
	placing these caches into el. I also tried putting these to stack and that was
	about same speed as static.
	*/
	static Vec3 vectorcache[ELLIPSOID_PIXELS_VERTICALLY + 1][ELLIPSOID_PIXELS_AROUND];
	static bool sidecache[ELLIPSOID_PIXELS_VERTICALLY + 1][ELLIPSOID_PIXELS_AROUND];

	// splane is in unit ball coordinates
	struct Plane splane = get_splitter_plane(el, cam);

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

			sidecache[v][a] = plane_whichside(splane, (*usvecs)[v][a]);

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
				SDL_FillRect(cam->surface, &rect, el->image[v][a]);
			}
		}
	}
}

static bool ellipsoid_intersects_plane(const struct Ellipsoid *el, struct Plane pl)
{
	// Switch to coordinates where ellipsoid is unit ball (a ball with radius 1)
	Vec3 center = mat3_mul_vec3(el->transform_inverse, el->center);
	plane_apply_mat3_INVERSE(&pl, el->transform);

	return (plane_point_distanceSQUARED(pl, center) < 1);
}

bool ellipsoid_visible(const struct Ellipsoid *el, const struct Camera *cam)
{
	for (unsigned i = 0; i < sizeof(cam->visibilityplanes)/sizeof(cam->visibilityplanes[0]); i++) {
		if (!plane_whichside(cam->visibilityplanes[i], el->center) && !ellipsoid_intersects_plane(el, cam->visibilityplanes[i]))
			return false;
	}
	return true;
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

float ellipsoid_bump_amount(const struct Ellipsoid *el1, const struct Ellipsoid *el2)
{
	Vec3 diff = vec3_sub(el1->center, el2->center);
	float difflen = hypotf(diff.x, diff.z);   // ignore diff.y
	if (difflen < 1e-5f) {
		// ellipses very near each other
		return el1->xzradius + el2->xzradius;
	}

	// Rotate centers so that ellipsoid centers have same z coordinate
	Mat3 rot = mat3_inverse(mat3_rotation_xz_sincos(diff.z/difflen, diff.x/difflen));
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
