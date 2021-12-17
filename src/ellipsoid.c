#include "ellipsoid.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "log.h"
#include "mathstuff.h"
#include "rect.h"

static bool ellipsoid_intersects_plane(const struct Ellipsoid *el, struct Plane pl)
{
	// Switch to coordinates where ellipsoid is unit ball (a ball with radius 1)
	Vec3 center = mat3_mul_vec3(el->transform_inverse, el->center);
	plane_apply_mat3_INVERSE(&pl, el->transform);

	return (plane_point_distanceSQUARED(pl, center) < 1);
}

bool ellipsoid_is_visible(const struct Ellipsoid *el, const struct Camera *cam)
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
	return true;
}

static SDL_Rect bounding_box_without_hidelowerhalf(
	const struct Ellipsoid *el, const struct Camera *cam)
{
	/*
	Each y coordinate on screen corresponds with a plane y/z = yzr, where yzr is a constant.
	To find them, we switch to coordinates where the ellipsoid is simply x^2+y^2+z^2=1.
	Let's call these "unit ball coordinates".
	*/
	Mat3 uball2world = el->transform;
	Mat3 uball2cam = mat3_mul_mat3(cam->world2cam, uball2world);

	/*
	At min and max screen y values, the distance between the plane and (0,0,0) is 1.
	If the plane is ax+by+cz+d=0, this gives

		|d| / sqrt(a^2 + b^2 + c^2) = 1.

	Solving yzr leads to a quadratic.
	The variant of the quadratic formula used:

		x^2 - 2bx + c = 0  <=>  x = b +- sqrt(b^2 - c)

	The same calculation works with x coordinates instead of y coordinates.
	*/
	Vec3 top = { uball2cam.rows[0][0], uball2cam.rows[0][1], uball2cam.rows[0][2] };
	Vec3 mid = { uball2cam.rows[1][0], uball2cam.rows[1][1], uball2cam.rows[1][2] };
	Vec3 bot = { uball2cam.rows[2][0], uball2cam.rows[2][1], uball2cam.rows[2][2] };
	float toptop = vec3_dot(top, top);
	float midmid = vec3_dot(mid, mid);
	float botbot = vec3_dot(bot, bot);
	float topbot = vec3_dot(top, bot);
	float midbot = vec3_dot(mid, bot);
	Vec3 center = camera_point_world2cam(cam, el->center);

	int xmin, xmax, ymin, ymax;
	{
		float a = botbot - center.z*center.z;
		float b = topbot - center.x*center.z;
		float c = toptop - center.x*center.x;
		b /= a;
		c /= a;
		SDL_assert(b*b-c >= 0);  // doesn't seem to ever be sqrt(negative)
		float offset = sqrtf(b*b-c);
		xmin = (int)camera_xzr_to_screenx(cam, b+offset);
		xmax = (int)camera_xzr_to_screenx(cam, b-offset);
	}
	{
		float a = botbot - center.z*center.z;
		float b = midbot - center.y*center.z;
		float c = midmid - center.y*center.y;
		b /= a;
		c /= a;
		SDL_assert(b*b-c >= 0);  // doesn't seem to ever be sqrt(negative)
		float offset = sqrtf(b*b-c);
		ymin = (int)camera_yzr_to_screeny(cam, b-offset);
		ymax = (int)camera_yzr_to_screeny(cam, b+offset);
	}

	SDL_Rect res;
	SDL_IntersectRect(
		&(SDL_Rect){ 0, 0, cam->surface->w, cam->surface->h },
		&(SDL_Rect){ xmin, ymin, xmax-xmin, ymax-ymin },
		&res);
	return res;
}

static SDL_Rect bounding_box_of_middle_circle(
	const struct Ellipsoid *el, const struct Camera *cam)
{
	/*
	Similar to bounding_box_without_hidelowerhalf.
	Doing this with the 2D circle in the middle (y=0 in unit ball coords)
	basically leads to the same equations, but in 2D.
	*/
	Mat3 uball2world = el->transform;
	Mat3 uball2cam = mat3_mul_mat3(cam->world2cam, uball2world);

	Vec2 top = { uball2cam.rows[0][0], uball2cam.rows[0][2] };
	Vec2 mid = { uball2cam.rows[1][0], uball2cam.rows[1][2] };
	Vec2 bot = { uball2cam.rows[2][0], uball2cam.rows[2][2] };
	Vec3 center = camera_point_world2cam(cam, el->center);
	float toptop = vec2_dot(top, top);
	float midmid = vec2_dot(mid, mid);
	float botbot = vec2_dot(bot, bot);
	float topbot = vec2_dot(top, bot);
	float midbot = vec2_dot(mid, bot);

	int xmin, xmax, ymin, ymax;
	{
		float a = botbot - center.z*center.z;
		float b = topbot - center.x*center.z;
		float c = toptop - center.x*center.x;
		b /= a;
		c /= a;
		SDL_assert(b*b-c >= 0);  // doesn't seem to ever be sqrt(negative)
		float offset = sqrtf(b*b-c);
		xmin = (int)camera_yzr_to_screeny(cam, b-offset);
		xmax = (int)camera_yzr_to_screeny(cam, b+offset);
	}
	{
		float a = botbot - center.z*center.z;
		float b = midbot - center.y*center.z;
		float c = midmid - center.y*center.y;
		b /= a;
		c /= a;
		SDL_assert(b*b-c >= 0);  // doesn't seem to ever be sqrt(negative)
		float offset = sqrtf(b*b-c);
		ymin = (int)camera_yzr_to_screeny(cam, b-offset);
		ymax = (int)camera_yzr_to_screeny(cam, b+offset);
	}

	SDL_Rect res;
	SDL_IntersectRect(
		&(SDL_Rect){ 0, 0, cam->surface->w, cam->surface->h },
		&(SDL_Rect){ xmin, ymin, xmax-xmin, ymax-ymin },
		&res);
	return res;
}

SDL_Rect ellipsoid_bounding_box(const struct Ellipsoid *el, const struct Camera *cam)
{

	SDL_Rect mainbbox = bounding_box_without_hidelowerhalf(el, cam);
	if (!el->epic->hidelowerhalf)
		return mainbbox;

	SDL_Rect circlebbox = bounding_box_of_middle_circle(el, cam);
	return (SDL_Rect){
		.x = mainbbox.x,
		.y = mainbbox.y,
		.w = mainbbox.w,
		.h = circlebbox.y + circlebbox.h - mainbbox.y,
	};
}

struct Rect ellipsoid_get_sort_rect(const struct Ellipsoid *el, const struct Camera *cam)
{
	Vec3 center2cam = vec3_sub(cam->location, el->center);
	Vec3 dir = { center2cam.z, 0, -center2cam.x };
	Vec3 center2edge = vec3_withlength(dir, 0.95f*el->xzradius);  // TODO: 0.95f still needed?

	Vec3 topleft, topright, botleft, botright;
	topleft = botleft = vec3_sub(el->center, center2edge);
	topright = botright = vec3_add(el->center, center2edge);
	topleft.y += el->yradius;
	topright.y += el->yradius;
	botleft.y -= el->yradius;
	botright.y -= el->yradius;
	return (struct Rect){.corners={ topleft, topright, botright, botleft }};
}

static void get_middle_circle_xzr_minmax(const struct Ellipsoid *el, const struct Camera *cam, int y, float *xzrmin, float *xzrmax)
{
	float yzr = camera_screeny_to_yzr(cam, y);

	// Find intersection of y/z = yzr (camera coords) and y=0 (unit ball coords)
	struct Plane yzrplane = { .normal = {0,1,-yzr}, .constant = 0 };
	plane_apply_mat3_INVERSE(&yzrplane, cam->world2cam);
	plane_move(&yzrplane, vec3_sub(el->center, cam->location));
	plane_apply_mat3_INVERSE(&yzrplane, el->transform);

	// Equation of yzrplane: (x,y,z) dot (a,b,c) = k
	float a = yzrplane.normal.x;
	float c = yzrplane.normal.z;
	float k = yzrplane.constant;

	// Intersecting yzrplane with y=0 gives a line (x,y,z) = p + t*dir
	float inv = 1/(a*a + c*c);
	Vec3 p = { a*k*inv, 0, c*k*inv };
	Vec3 dir = { -c, 0, a };

	// Solve t = +-abst so that intersection is on the unit ball x^2+y^2+z^2=1
	SDL_assert(k*k*inv <= 1);
	float abst = sqrtf(inv - k*k*inv*inv);

	Vec3 p1 = vec3_add(p, vec3_mul_float(dir, -abst));
	Vec3 p2 = vec3_add(p, vec3_mul_float(dir, abst));
	vec3_apply_matrix(&p1, el->transform);
	vec3_apply_matrix(&p2, el->transform);
	vec3_add_inplace(&p1, vec3_sub(cam->location, el->center));
	vec3_add_inplace(&p2, vec3_sub(cam->location, el->center));
	vec3_apply_matrix(&p1, cam->world2cam);
	vec3_apply_matrix(&p2, cam->world2cam);
	float xzr1 = p1.x / p1.z;
	float xzr2 = p2.x / p2.z;
	SDL_assert(xzr1 < xzr2);
	*xzrmin = xzr1;
	*xzrmax = xzr2;
	SDL_assert(*xzrmin <= *xzrmax);
}

bool ellipsoid_xminmax(const struct Ellipsoid *el, const struct Camera *cam, int y, int *xmin, int *xmax)
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

		f(t) = (p + t(xzr*v + w)) dot (p + t(xzr*v + w)).

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
	float xzrleft = -b+offset;
	float xzrright = -b-offset;

	if (el->epic->hidelowerhalf) {
		// Find y coords of corresponding points on the unit ball (bl left side, br right side)
		Vec3 ul = vec3_add(vec3_mul_float(v, xzrleft), w);
		Vec3 ur = vec3_add(vec3_mul_float(v, xzrright), w);
		float yl = p.y - vec3_dot(p,ul)/vec3_dot(ul,ul)*ul.y;
		float yr = p.y - vec3_dot(p,ur)/vec3_dot(ur,ur)*ur.y;

		// If below, use unit circle point instead
		if (yl < 0 || yr < 0) {
			float l, r;
			get_middle_circle_xzr_minmax(el, cam, y, &r, &l);
			if (yl < 0) xzrleft = l;
			if (yr < 0) xzrright = r;
		}
	}

	*xmin = (int)camera_xzr_to_screenx(cam, xzrleft);
	*xmax = (int)camera_xzr_to_screenx(cam, xzrright);
	clamp(xmin, 0, cam->surface->w);
	clamp(xmax, 0, cam->surface->w);
	return *xmin <= *xmax;
}

static inline float linear_map(float srcmin, float srcmax, float dstmin, float dstmax, float val)
{
	// ratio should get inlined when everything except val is constants
	float ratio = (dstmax - dstmin)/(srcmax - srcmin);
	return dstmin + (val - srcmin)*ratio;
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
