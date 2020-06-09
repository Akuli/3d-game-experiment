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

static float linear_map(float srcmin, float srcmax, float dstmin, float dstmax, float val)
{
	float ratio = (val - srcmin)/(srcmax - srcmin);
	return dstmin + ratio*(dstmax - dstmin);
}

// about 2x faster than SDL_FillRect(surf, &(SDL_Rect){x,y,1,1}, px)
static inline void set_pixel(SDL_Surface *surf, int x, int y, uint32_t px)
{
	unsigned char *ptr = surf->pixels;
	ptr += y*surf->pitch;
	ptr += x*(int)sizeof(px);
	memcpy(ptr, &px, sizeof(px));   // no strict aliasing issues thx
}

void ellipsoid_show(const struct Ellipsoid *el, const struct Camera *cam)
{
	// switching to unit ball coordinates
	Vec3 camloc = mat3_mul_vec3(el->transform_inverse, vec3_sub(cam->location, el->center));
	Mat3 cam2unitb = mat3_mul_mat3(el->transform_inverse, mat3_inverse(cam->world2cam));
	Mat3 unitb2cam = mat3_inverse(cam2unitb);

	if (vec3_lengthSQUARED(camloc) <= 1) {
		// camera inside ball
		return;
	}

	float pi = acosf(-1);

	float pp = vec3_lengthSQUARED(camloc);

	for (int x = 0; x < cam->surface->w; x++) {
		float xzr = camera_screenx_to_xzr(cam, (float)x);

		/*
		Vertical plane representing everything seen with this x coordinate:

			x/z = xzr
			1x + 0y + (-xzr)*z = 0
			(-1)x + 0y + xzr*z = 0

		This is in camera coordinates. The normal direction has to be right,
		that was done with trial and error (lol)
		*/
		struct Plane xplane = { .normal = {-1, 0, xzr}, .constant = 0 };
		plane_apply_mat3_INVERSE(&xplane, unitb2cam);
		assert(xplane.constant == 0);
		plane_move(&xplane, camloc);

		// does it even touch the unit ball?
		float plane2centerSQUARED = plane_point_distanceSQUARED(xplane, (Vec3){0,0,0});
		if (plane2centerSQUARED > 1)
			continue;

		// The intersection is a circle
		float radiusSQUARED = 1 - plane2centerSQUARED;   // Pythagorean theorem
		Vec3 icenter = vec3_withlength(xplane.normal, sqrtf(plane2centerSQUARED));

		Vec3 icenter2cam = vec3_sub(camloc, icenter);

		// TODO: explain the "line" that i'm thinking about here
		float littlebit = radiusSQUARED / sqrtf(vec3_lengthSQUARED(vec3_sub(camloc, icenter)));   // similar triangles
		Vec3 icenter2linecenter = vec3_withlength(icenter2cam, littlebit);
		Vec3 linecenter = vec3_add(icenter, icenter2linecenter);

		float linelenHALF = sqrtf(radiusSQUARED - littlebit*littlebit);   // pythagorean theorem
		Vec3 linevector = vec3_withlength(vec3_cross(icenter2cam, xplane.normal), linelenHALF);
		Vec3 barelytouches1 = vec3_add(linecenter, linevector);
		Vec3 barelytouches2 = vec3_sub(linecenter, linevector);

		vec3_add_inplace(&barelytouches1, vec3_neg(camloc));
		vec3_add_inplace(&barelytouches2, vec3_neg(camloc));
		vec3_apply_matrix(&barelytouches1, unitb2cam);
		vec3_apply_matrix(&barelytouches2, unitb2cam);

		int ylower = (int)camera_yzr_to_screeny(cam, barelytouches2.y / barelytouches2.z);
		int yupper = (int)camera_yzr_to_screeny(cam, barelytouches1.y / barelytouches1.z);

		if (ylower < 0)
			ylower = 0;
		if (yupper >= cam->surface->h)
			yupper = cam->surface->h - 1;

		for (int y = ylower; y < yupper; y++) {
			float yzr = camera_screeny_to_yzr(cam, (float)y);

			/*
			line equation in camera coordinates:

			x = xzr*z, y = yzr*z aka (x,y,z) = z*(xzr,yzr,1)

			just need to convert that to unit ball coords
			*/
			Vec3 linedir = mat3_mul_vec3(cam2unitb, (Vec3){xzr,yzr,1});

			/*
			Intersecting the unit ball

				(x,y,z) dot (x,y,z) = 1

			with the line

				(x,y,z) = camloc + t*linedir

			creates a system of equations. We want the solution with bigger t,
			because the direction vector has z component 1 in camera coordinates,
			i.e. it's pointing towards the camera.
			*/
			float dd = vec3_lengthSQUARED(linedir);
			float pd = vec3_dot(camloc, linedir);

			float discriminant = pd*pd - (pp - 1)*dd;
			if (discriminant < 0) {
				// nothing for this pixel
				//log_printf("negative under sqrt %f", discriminant);
				continue;
			}

			float t = (-pd + sqrtf(discriminant))/dd;
			Vec3 vec = vec3_add(camloc, vec3_mul_float(linedir, t));
			if (el->hidelowerhalf && vec.y < 0)
				continue;

			float xzangle = atan2f(vec.z, vec.x);
			if (xzangle < 0)
				xzangle += 2*pi;

			int a = (int)linear_map(0, 2*pi, 0, ELLIPSOID_PIXELS_AROUND,	xzangle);
			if (a < 0)
				a = 0;
			if (a >= ELLIPSOID_PIXELS_AROUND)
				a = ELLIPSOID_PIXELS_AROUND - 1;

			int v = (int)linear_map(1, -1, 0, ELLIPSOID_PIXELS_VERTICALLY,	vec.y);
			if (v < 0)
				v = 0;
			if (v >= ELLIPSOID_PIXELS_VERTICALLY)
				v = ELLIPSOID_PIXELS_VERTICALLY - 1;

			set_pixel(cam->surface, x, y, el->image[v][a]);
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
