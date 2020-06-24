#include "ellipsoid.h"
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "log.h"
#include "ellipsemove.h"
#include "mathstuff.h"
#include "showall.h"

static bool ellipsoid_intersects_plane(const struct Ellipsoid *el, struct Plane pl)
{
	// Switch to coordinates where ellipsoid is unit ball (a ball with radius 1)
	Vec3 center = mat3_mul_vec3(el->transform_inverse, el->center);
	plane_apply_mat3_INVERSE(&pl, el->transform);

	return (plane_point_distanceSQUARED(pl, center) < 1);
}

/*
Given a circle on a plane going through (0,0,0), find the points A and B where
tangent lines of circle going through (0,0,0) intersect the circle
*/
static void tangent_line_intersections(
	Vec3 planenormal, Vec3 center, float radiusSQUARED, Vec3 *A, Vec3 *B)
{
	/*
		       o o
		    o       o
		  o           o
		\o    center   o/
		 \A-----C-----B/
		  \ o       o /
		   \   o o   /
		    \       /
		     \     /
		      \   /
		       \ /
		     (0,0,0)

	The \ and / lines are tangent lines of the intersection circle going
	through the camera. We want to find those, because they correspond to
	the first and last y pixel of the ball visible to the camera. Letting
	A and B denote the intersections of the tangent lines with the circle,
	and letting C denote the center of line AB, we see that the triangles

		B-C-center  and  camera-B-center

	are similar (90deg angle and the angle at center in both). Hence

		|C - center| / |center - B| = |B - center| / |center - camera|

	which gives

		|C - center| = radius^2 / |center|.

	The unit vector in direction of center (thinking of center as a
	vector from camera=(0,0,0) to center) is center/|center|, so

		C = center/|center| * (|center| - radius^2/|center|)
		  = center * (1 - radius^2/|center|^2)
	*/
	Vec3 C = vec3_mul_float(center, 1 - radiusSQUARED/vec3_lengthSQUARED(center));

	/*
	Pythagorean theorem: radius^2 = |C-A|^2 + |C - center|^2

	Plugging in from above gives this. Note that it has radius^4, aka
	radius^2 * radius^2.
	*/
	float distanceCA = sqrtf(radiusSQUARED - radiusSQUARED*radiusSQUARED/vec3_lengthSQUARED(center));

	/*
	Use right hand rule for cross product, with normal vector
	pointing from you towards your screen in above picture.

	The pointing direction could also be the opposite and you would get
	CtoB instead of CtoA. Doesn't matter.
	*/
	Vec3 CtoA = vec3_withlength(vec3_cross(center, planenormal), distanceCA);
	*A = vec3_add(C, CtoA);
	*B = vec3_sub(C, CtoA);
}

bool ellipsoid_visible_xminmax(
	const struct Ellipsoid *el, const struct Camera *cam, int *xmin, int *xmax)
{
	/*
	Ensure that it's in front of camera and not even touching the
	camera plane. This allows us to make nice assumptions:
		- Camera is not inside ellipsoid
		- x/z ratios of all points on ellipsoid surface in camera coords work
	*/
	if (!plane_whichside(cam->visplanes[CAMERA_CAMPLANE_IDX], el->center) ||
		ellipsoid_intersects_plane(el, cam->visplanes[CAMERA_CAMPLANE_IDX]))
	{
		return false;
	}

	for (int i = 0; i < sizeof(cam->visplanes)/sizeof(cam->visplanes[0]); i++) {
		if (i == CAMERA_CAMPLANE_IDX)
			continue;

		/*
		If center is on the wrong side, then it can touch the plane to
		still be partially visible
		*/
		if (!plane_whichside(cam->visplanes[i], el->center) &&
			!ellipsoid_intersects_plane(el, cam->visplanes[i]))
		{
			return false;
		}
	}

	// switch to camera coordinates
	Vec3 center = camera_point_world2cam(cam, el->center);

	// A non-tilted plane (i.e. planenormal.x = 0) going through camera at (0,0,0) and center
	struct Plane pl = { .normal = { 0, center.z, -center.y }, .constant = 0 };

	// switch to coordinates with unit ball
	vec3_apply_matrix(&center, el->transform_inverse);
	plane_apply_mat3_INVERSE(&pl, el->transform);

	Vec3 A, B;
	tangent_line_intersections(pl.normal, center, 1*1, &A, &B);

	// back to camera coordinates
	vec3_apply_matrix(&A, el->transform);
	vec3_apply_matrix(&B, el->transform);

	// trial and error has been used to figure out which is bigger
	*xmin = (int)ceilf(camera_point_cam2screen(cam, A).x);
	*xmax = (int)      camera_point_cam2screen(cam, B).x;
	return true;
}

static void fill_xcache(
	const struct Ellipsoid *el, const struct Camera *cam,
	int x, struct EllipsoidXCache *xcache)
{
	xcache->x = x;
	xcache->cam = cam;
	xcache->xzr = camera_screenx_to_xzr(cam, (float)x);

	// Equation of plane of points having this screen x:
	// x/z = xzr, aka 1x + 0y + (-xzr)z = 0
	xcache->xplane = (struct Plane) { .normal = {1, 0, -xcache->xzr}, .constant = 0 };
	plane_apply_mat3_INVERSE(&xcache->xplane, el->transform);

	xcache->ballcenter = mat3_mul_vec3(
		el->transform_inverse, camera_point_world2cam(cam, el->center));

	xcache->dSQUARED = plane_point_distanceSQUARED(xcache->xplane, xcache->ballcenter);
	if (xcache->dSQUARED >= 1) {
		log_printf("hopefully this is near 1: %f", xcache->dSQUARED);
		xcache->dSQUARED = 1;
	}
}

static void calculate_yminmax_without_hidelowerhalf(const struct Ellipsoid *el, const struct EllipsoidXCache *xcache, int *ymin, int *ymax)
{
	// Intersection of xplane and unit ball is a circle
	float radiusSQUARED = 1 - xcache->dSQUARED;   // Pythagorean theorem
	Vec3 center = vec3_add(xcache->ballcenter, vec3_withlength(xcache->xplane.normal, sqrtf(xcache->dSQUARED)));

	// camera is at (0,0,0) and tangent lines correspond to first and last visible y pixel of the ball
	Vec3 A, B;
	tangent_line_intersections(xcache->xplane.normal, center, radiusSQUARED, &A, &B);

	// Trial and error has been used to figure out which is bigger and which is smaller...
	*ymax = (int)ceilf(camera_point_cam2screen(xcache->cam, mat3_mul_vec3(el->transform, A)).y);
	*ymin = (int)      camera_point_cam2screen(xcache->cam, mat3_mul_vec3(el->transform, B)).y;
	SDL_assert(*ymin <= *ymax);
}

static int calculate_center_y(const struct Ellipsoid *el, const struct EllipsoidXCache *xcache)
{
	SDL_assert(xcache->xplane.constant == 0);  // goes through camera at (0,0,0)
	SDL_assert(xcache->xplane.normal.y == 0);  // not tilted

	/*
	We want to intersect these:
	- unit ball: (x - ballcenter.x)^2 + (y - ballcenter.y)^2 + (z - ballcenter.z)^2 = 1
	- plane that splits unit ball in half: y = ballcenter.y
	- xplane: x*xplane.normal.x + z*xplane.normal.z = 0

	Intersecting all that gives 3 equations with 3 unknowns x,y,z.
	*/

	float bottom = vec3_lengthSQUARED(xcache->xplane.normal);
	float dot = vec3_dot(xcache->xplane.normal, xcache->ballcenter);
	float undersqrt = bottom - dot*dot;
	if (undersqrt < 0)
		undersqrt = 0;

	float nx = xcache->xplane.normal.x;
	float nz = xcache->xplane.normal.z;
	float bx = xcache->ballcenter.x;
	float bz = xcache->ballcenter.z;
	float infrontofsqrt = nx*bz - bx*nz;

	// Choosing +sqrt seems to always work
	float tmp = (infrontofsqrt + sqrtf(undersqrt))/bottom;
	Vec3 v = { -nz*tmp, xcache->ballcenter.y, nx*tmp };

	vec3_apply_matrix(&v, el->transform);
	return (int)camera_yzr_to_screeny(xcache->cam, v.y/v.z);
}

void ellipsoid_yminmax(
	const struct Ellipsoid *el, const struct Camera *cam,
	int x, struct EllipsoidXCache *xcache,
	int *ymin, int *ymax)
{
	fill_xcache(el, cam, x, xcache);
	calculate_yminmax_without_hidelowerhalf(el, xcache, ymin, ymax);
	if (el->epic->hidelowerhalf)
		*ymax = calculate_center_y(el, xcache);

	*ymin = max(0, min(*ymin, xcache->cam->surface->h - 1));
	*ymax = max(0, min(*ymax, xcache->cam->surface->h - 1));
}

// about 2x faster than SDL_FillRect(surf, &(SDL_Rect){x,y,1,1}, px)
static inline void set_pixel(SDL_Surface *surf, int x, int y, uint32_t px)
{
	unsigned char *ptr = surf->pixels;
	ptr += y*surf->pitch;
	ptr += x*sizeof(px);
	memcpy(ptr, &px, sizeof(px));   // no strict aliasing issues thx
}

static inline float linear_map(float srcmin, float srcmax, float dstmin, float dstmax, float val)
{
	// ratio should get inlined when everything except val is constants
	float ratio = (dstmax - dstmin)/(srcmax - srcmin);
	return dstmin + (val - srcmin)*ratio;
}

void ellipsoid_drawcolumn(
	const struct Ellipsoid *el, const struct EllipsoidXCache *xcache,
	int ymin, int ymax)
{
	int ydiff = ymax - ymin;
	if (ydiff <= 0)
		return;
	SDL_assert(0 <= ymin && ymin+ydiff <= xcache->cam->surface->h);
	SDL_assert(ydiff <= CAMERA_SCREEN_HEIGHT);

	/*
	Code is ugly but gcc vectorizes it to make it very fast. This code was the
	bottleneck of the game before making it more vectorizable, and it still is
	at the time of writing this comment.
	*/

#define LOOP for(int i = 0; i < ydiff; i++)

	float yzr[CAMERA_SCREEN_HEIGHT];
	LOOP yzr[i] = camera_screeny_to_yzr(xcache->cam, (float)(ymin + i));

	/*
	line equation in camera coordinates:

		x = xzr*z, y = yzr*z aka (x,y,z) = z*(xzr,yzr,1)

	Note that this has z coordinate 1 in camera coordinates, i.e. pointing toward camera
	*/
	float linedirx[CAMERA_SCREEN_HEIGHT], linediry[CAMERA_SCREEN_HEIGHT], linedirz[CAMERA_SCREEN_HEIGHT];
	LOOP linedirx[i] = mat3_mul_vec3(el->transform_inverse, (Vec3){xcache->xzr,yzr[i],1}).x;
	LOOP linediry[i] = mat3_mul_vec3(el->transform_inverse, (Vec3){xcache->xzr,yzr[i],1}).y;
	LOOP linedirz[i] = mat3_mul_vec3(el->transform_inverse, (Vec3){xcache->xzr,yzr[i],1}).z;
#define LineDir(i) ( (Vec3){ linedirx[i], linediry[i], linedirz[i] } )

	/*
	Let xyz denote the vector (x,y,z). Intersecting the ball

		(xyz - ballcenter) dot (xyz - ballcenter) = 1

	with the line

		xyz = t*linedir

	creates a quadratic equation in t. We want the solution with bigger t,
	because the direction vector is pointing towards the camera.
	*/
	float cc = vec3_lengthSQUARED(xcache->ballcenter);    // ballcenter dot ballcenter
	float dd[CAMERA_SCREEN_HEIGHT];    // linedir dot linedir
	float cd[CAMERA_SCREEN_HEIGHT];    // ballcenter dot linedir
	LOOP dd[i] = vec3_lengthSQUARED(LineDir(i));
	LOOP cd[i] = vec3_dot(xcache->ballcenter, LineDir(i));

	float t[CAMERA_SCREEN_HEIGHT];
	LOOP t[i] = cd[i]*cd[i] - dd[i]*(cc-1);
	LOOP t[i] = max(0, t[i]);   // no negative under sqrt plz. Don't know why t[i] can be more than just a little bit negative...
	LOOP t[i] = (cd[i] + sqrtf(t[i]))/dd[i];

	float vecx[CAMERA_SCREEN_HEIGHT], vecy[CAMERA_SCREEN_HEIGHT], vecz[CAMERA_SCREEN_HEIGHT];
	LOOP vecx[i] = mat3_mul_vec3(xcache->cam->cam2world, vec3_sub(vec3_mul_float(LineDir(i), t[i]), xcache->ballcenter)).x;
	LOOP vecy[i] = mat3_mul_vec3(xcache->cam->cam2world, vec3_sub(vec3_mul_float(LineDir(i), t[i]), xcache->ballcenter)).y;
	LOOP vecz[i] = mat3_mul_vec3(xcache->cam->cam2world, vec3_sub(vec3_mul_float(LineDir(i), t[i]), xcache->ballcenter)).z;
#undef LineDir

	int ex[CAMERA_SCREEN_HEIGHT], ey[CAMERA_SCREEN_HEIGHT], ez[CAMERA_SCREEN_HEIGHT];
	LOOP ex[i] = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE, vecx[i]);
	LOOP ey[i] = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE, vecy[i]);
	LOOP ez[i] = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE, vecz[i]);

	LOOP ex[i] = max(0, min(ELLIPSOIDPIC_SIDE-1, ex[i]));
	LOOP ey[i] = max(0, min(ELLIPSOIDPIC_SIDE-1, ey[i]));
	LOOP ez[i] = max(0, min(ELLIPSOIDPIC_SIDE-1, ez[i]));

	uint32_t px[CAMERA_SCREEN_HEIGHT];
	LOOP px[i] = el->epic->cubepixels[ex[i]][ey[i]][ez[i]];
	LOOP set_pixel(xcache->cam->surface, xcache->x, ymin + i, px[i]);
#undef LOOP
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
		// centers very near each other
		return el1->xzradius + el2->xzradius;
	}

	// Rotate centers so that ellipsoid centers have same z coordinate
	Mat3 rot = mat3_inverse(mat3_rotation_xz_sincos(diff.z/difflen, diff.x/difflen));
	Vec3 center1 = mat3_mul_vec3(rot, el1->center);
	Vec3 center2 = mat3_mul_vec3(rot, el2->center);
	SDL_assert(fabsf(center1.z - center2.z) < 1e-5f);

	// Now this is a 2D problem on the xy plane (or some other plane parallel to xy plane)
	Vec2 center1_xy = { center1.x, center1.y };
	Vec2 center2_xy = { center2.x, center2.y };
	return ellipse_move_amount_x(
		el1->xzradius, el1->yradius, center1_xy,
		el2->xzradius, el2->yradius, center2_xy);
}

void ellipsoid_move_apart(struct Ellipsoid *el1, struct Ellipsoid *el2, float mv)
{
	SDL_assert(mv >= 0);
	Vec3 from1to2 = vec3_sub(el2->center, el1->center);
	from1to2.y = 0;   // don't move in y direction
	if (vec3_lengthSQUARED(from1to2) < 1e-5f) {
		/*
		I have never seen this actually happening, because this function prevents
		going under another player. Players could be also lined up by jumping
		over another player and having the luck to get it perfectly aligned...
		*/
		log_printf("ellipsoids line up in y direction, doing dumb thing to avoid divide by zero");
		from1to2 = (Vec3){1,0,0};
	}

	from1to2 = vec3_withlength(from1to2, mv/2);
	vec3_add_inplace(&el2->center, from1to2);
	vec3_sub_inplace(&el1->center, from1to2);
}
