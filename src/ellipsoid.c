#include "ellipsoid.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "camera.h"
#include "log.h"
#include "ellipsemove.h"
#include "mathstuff.h"

// This file uses unsigned char instead of uint8_t because it works better with strict aliasing

/*
We introduce third type of coordinates: unit ball coordinates
- ellipsoid center is at (0,0,0)
- ellipsoid radius is 1
- cam->world2cam transform hasn't been applied yet
*/

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
	vec3_sub_inplace(&el1->center, from1to2);
}
