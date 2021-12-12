#include "ellipsoid.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "log.h"
#include "mathstuff.h"

static bool ellipsoid_intersects_plane(const struct Ellipsoid *el, struct Plane pl)
{
	// Switch to coordinates where ellipsoid is unit ball (a ball with radius 1)
	Vec3 center = mat3_mul_vec3(el->transform_inverse, el->center);
	plane_apply_mat3_INVERSE(&pl, el->transform);

	return (plane_point_distanceSQUARED(pl, center) < 1);
}

bool ellipsoid_yminmax_new(const struct Ellipsoid *el, const struct Camera *cam, int *ymin, int *ymax)
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

	/*
	Each y coordinate on screen corresponds with a plane y/z = yzr, where yzr is a constant.
	To find them, we switch to coordinates where the ellipsoid is simply x^2+y^2+z^2=1.
	Let's call these "unit ball coordinates".
	*/
	Mat3 uball2cam = mat3_mul_mat3(cam->world2cam, el->transform);

	/*
	At min and max y values, the distance between the plane and (0,0,0) is 1.
	If the plane is ax+by+cz+d=0, this gives

		|d| / sqrt(a^2 + b^2 + c^2) = 1.

	Solving yzr leads to a quadratic.
	The variant of the quadratic formula used:

		x^2 - 2bx + c = 0  <=>  x = b +- sqrt(b^2 - c)
	*/
	Vec3 mid = { uball2cam.rows[1][0], uball2cam.rows[1][1], uball2cam.rows[1][2] };
	Vec3 bot = { uball2cam.rows[2][0], uball2cam.rows[2][1], uball2cam.rows[2][2] };
	float midmid = vec3_dot(mid, mid);
	float midbot = vec3_dot(mid, bot);
	float botbot = vec3_dot(bot, bot);
	Vec3 center = camera_point_world2cam(cam, el->center);
	float a = botbot - center.z*center.z;
	float b = midbot - center.y*center.z;
	float c = midmid - center.y*center.y;
	b /= a;
	c /= a;
	SDL_assert(b*b-c >= 0);  // doesn't seem to ever be sqrt(negative)
	float offset = sqrtf(b*b-c);

	*ymin = (int)camera_yzr_to_screeny(cam, b-offset);
	*ymax = (int)camera_yzr_to_screeny(cam, b+offset);
	clamp(ymin, 0, cam->surface->h);
	clamp(ymax, 0, cam->surface->h);
	return true;
}

bool ellipsoid_xminmax_new(const struct Ellipsoid *el, const struct Camera *cam, int y, int *xmin, int *xmax)
{
	/*
	Consider the line that is t*(xzr,yzr,1) in camera coordinates.
	In unit ball coordinates, it will be

		t*(xzr*v + w) + p,

	where v, w and p don't depend on xzr or t.
	*/
	Mat3 world2uball = el->transform_inverse;
	Mat3 cam2uball = mat3_mul_mat3(world2uball, cam->cam2world);
	Vec3 v = mat3_mul_vec3(cam2uball, (Vec3){1,0,0});
	Vec3 w = mat3_mul_vec3(cam2uball, (Vec3){0,camera_screeny_to_yzr(cam, y),1});
	Vec3 p = mat3_mul_vec3(world2uball, vec3_sub(cam->location, el->center));

	/*
	Consider the function

		f(t) = (c + t(xzr*v + w)) dot (c + t(xzr*v + w)).

	Its minimum value is (distance between line and origin)^2.
	Solve it with a derivative and set it equal to 1.
	Then solve xzr with quadratic formula.

	Variant of quadratic formula used:

		x^2 + 2bx + c = 0  <=>  x = -b +- sqrt(b^2 - c)
	*/
	float pp = vec3_dot(p, p);
	float vv = vec3_dot(v, v);
	float ww = vec3_dot(w, w);
	float pv = vec3_dot(p, v);
	float pw = vec3_dot(p, w);
	float vw = vec3_dot(v, w);
	float a = (pp-1)*vv - pv*pv;
	float b = (pp-1)*vw - pv*pw;
	float c = (pp-1)*ww - pw*pw;
	b /= a;
	c /= a;
	if (b*b-c < 0) return false;    // happens about once per frame
	float offset = sqrtf(b*b-c);

	*xmin = (int)camera_xzr_to_screenx(cam, -b+offset);
	*xmax = (int)camera_xzr_to_screenx(cam, -b-offset);
	clamp(xmin, 0, cam->surface->w);
	clamp(xmax, 0, cam->surface->w);
	return *xmin < *xmax;
}

void ellipsoid_debug_shit(const struct Ellipsoid *el, const struct Camera *cam)
{
	int ymin, ymax;
	if (ellipsoid_yminmax_new(el, cam, &ymin, &ymax)) {
		SDL_FillRect(cam->surface, &(SDL_Rect){0, ymax, cam->surface->w, 1}, (uint32_t)0x0000ff00UL);
		SDL_FillRect(cam->surface, &(SDL_Rect){0, ymin, cam->surface->w, 1}, (uint32_t)0x00ff0000UL);
		for (int y = ymin; y < ymax; y += 1) {
			int xmin, xmax;
			if (ellipsoid_xminmax_new(el, cam, y, &xmin, &xmax))
				SDL_FillRect(cam->surface, &(SDL_Rect){xmin, y, xmax-xmin, 1}, (uint32_t)0x000000ffUL);
		}
	}
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
	xcache->screenx = x;
	xcache->cam = cam;
	xcache->xzr = camera_screenx_to_xzr(cam, x);

	/*
	Equation of plane of points having this screen x:
	x/z = xzr, aka 1x + 0y + (-xzr)z = 0
	Note that xplane's normal vector always points towards positive camera x direction.
	*/
	xcache->xplane = (struct Plane) { .normal = { 1, 0, -xcache->xzr }, .constant = 0 };
	plane_apply_mat3_INVERSE(&xcache->xplane, el->transform);

	Vec3 ballcenter = camera_point_world2cam(cam, el->center);
	xcache->ballcenter = mat3_mul_vec3(el->transform_inverse, ballcenter);
	xcache->ballcenterscreenx = camera_point_cam2screen(xcache->cam, ballcenter).x;

	xcache->dSQUARED = plane_point_distanceSQUARED(xcache->xplane, xcache->ballcenter);
	if (xcache->dSQUARED >= 1) {
		log_printf("hopefully this is near 1: %f", xcache->dSQUARED);
		xcache->dSQUARED = 1;
	}
}

static void calculate_yminmax_without_hidelowerhalf(const struct Ellipsoid *el, const struct EllipsoidXCache *xcache, int *ymin, int *ymax)
{
	// center and radiusSQUARED describe intersection of xplane and unit ball, which is a circle
	float len = sqrtf(xcache->dSQUARED);
	if (xcache->screenx < xcache->ballcenterscreenx) {
		// xplane normal vector points to right, but we need to go left instead
		len = -len;
	}
	Vec3 center = vec3_add(xcache->ballcenter, vec3_withlength(xcache->xplane.normal, len));
	float radiusSQUARED = 1 - xcache->dSQUARED;   // Pythagorean theorem

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

	clamp(ymin, 0, xcache->cam->surface->h - 1);
	clamp(ymax, 0, xcache->cam->surface->h - 1);
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

	// just in case floats do something weird, e.g. division by zero
	LOOP clamp(&ex[i], 0, ELLIPSOIDPIC_SIDE-1);
	LOOP clamp(&ey[i], 0, ELLIPSOIDPIC_SIDE-1);
	LOOP clamp(&ez[i], 0, ELLIPSOIDPIC_SIDE-1);

	uint32_t px[CAMERA_SCREEN_HEIGHT];
	bool hl = el->highlighted;
	LOOP px[i] = el->epic->cubepixels[hl][ex[i]][ey[i]][ez[i]];
	LOOP set_pixel(xcache->cam->surface, xcache->screenx, ymin + i, px[i]);
#undef LOOP
}

void ellipsoid_drawrow(
	const struct Ellipsoid *el, const struct Camera *cam,
	int y, int xmin, int xmax)
{
	int xdiff = xmax - xmin;
	if (xdiff <= 0)
		return;
	SDL_assert(0 <= xmin && xmin+xdiff <= cam->surface->w);
	SDL_assert(xdiff <= CAMERA_SCREEN_WIDTH);

	SDL_assert(cam->surface->pitch % sizeof(uint32_t) == 0);
	int mypitch = cam->surface->pitch / sizeof(uint32_t);

	/*
	Code is ugly but gcc vectorizes it to make it very fast. This code was the
	bottleneck of the game before making it more vectorizable, and it still is
	at the time of writing this comment.
	*/

#define LOOP for(int i = 0; i < xdiff; i++)

	float xzr[CAMERA_SCREEN_WIDTH];
	LOOP xzr[i] = camera_screenx_to_xzr(cam, (float)(xmin + i));

	/*
	line equation in camera coordinates:

		x = xzr*z, y = yzr*z aka (x,y,z) = z*(xzr,yzr,1)

	Note that this has z coordinate 1 in camera coordinates, i.e. pointing toward camera.
	*/
	float yzr = camera_screeny_to_yzr(cam, y);
	float linedirx[CAMERA_SCREEN_WIDTH], linediry[CAMERA_SCREEN_WIDTH], linedirz[CAMERA_SCREEN_WIDTH];
	LOOP linedirx[i] = mat3_mul_vec3(el->transform_inverse, (Vec3){xzr[i],yzr,1}).x;
	LOOP linediry[i] = mat3_mul_vec3(el->transform_inverse, (Vec3){xzr[i],yzr,1}).y;
	LOOP linedirz[i] = mat3_mul_vec3(el->transform_inverse, (Vec3){xzr[i],yzr,1}).z;
#define LineDir(i) ( (Vec3){ linedirx[i], linediry[i], linedirz[i] } )

	/*
	Intersecting the ball

		((x,y,z) - ballcenter) dot ((x,y,z) - ballcenter) = 1

	with the line

		(x,y,z) = t*linedir

	creates a quadratic equation in t. We want the solution with bigger t,
	because the direction vector is pointing towards the camera.
	*/
	Vec3 ballcenter = mat3_mul_vec3(el->transform_inverse, camera_point_world2cam(cam, el->center));
	float cc = vec3_dot(ballcenter, ballcenter);
	float dd[CAMERA_SCREEN_WIDTH];
	float cd[CAMERA_SCREEN_WIDTH];
	LOOP dd[i] = vec3_dot(LineDir(i), LineDir(i));
	LOOP cd[i] = vec3_dot(ballcenter, LineDir(i));

	float t[CAMERA_SCREEN_WIDTH];
	LOOP t[i] = cd[i]*cd[i] - dd[i]*(cc-1);
	LOOP t[i] = max(0, t[i]);   // no negative under sqrt plz. Don't know why t[i] can be more than just a little bit negative...
	LOOP t[i] = (cd[i] + sqrtf(t[i]))/dd[i];

	float vecx[CAMERA_SCREEN_WIDTH], vecy[CAMERA_SCREEN_WIDTH], vecz[CAMERA_SCREEN_WIDTH];
	LOOP vecx[i] = mat3_mul_vec3(cam->cam2world, vec3_sub(vec3_mul_float(LineDir(i), t[i]), ballcenter)).x;
	LOOP vecy[i] = mat3_mul_vec3(cam->cam2world, vec3_sub(vec3_mul_float(LineDir(i), t[i]), ballcenter)).y;
	LOOP vecz[i] = mat3_mul_vec3(cam->cam2world, vec3_sub(vec3_mul_float(LineDir(i), t[i]), ballcenter)).z;
#undef LineDir

	int ex[CAMERA_SCREEN_WIDTH], ey[CAMERA_SCREEN_WIDTH], ez[CAMERA_SCREEN_WIDTH];
	LOOP ex[i] = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE, vecx[i]);
	LOOP ey[i] = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE, vecy[i]);
	LOOP ez[i] = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE, vecz[i]);

	// just in case floats do something weird, e.g. division by zero
	LOOP clamp(&ex[i], 0, ELLIPSOIDPIC_SIDE-1);
	LOOP clamp(&ey[i], 0, ELLIPSOIDPIC_SIDE-1);
	LOOP clamp(&ez[i], 0, ELLIPSOIDPIC_SIDE-1);

	uint32_t px[CAMERA_SCREEN_WIDTH];
	bool hl = el->highlighted;
	LOOP px[i] = el->epic->cubepixels[hl][ex[i]][ey[i]][ez[i]];
#undef LOOP

	// TODO: faster with or without memcpy?
	uint32_t *pixdst = (uint32_t *)cam->surface->pixels + mypitch*y + xmin;
	memcpy(pixdst, px, sizeof(uint32_t)*xdiff);
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
