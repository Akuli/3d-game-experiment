#include "ellipsoid.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "camera.h"
#include "linalg.h"
#include "log.h"
#include "map.h"
#include "misc.h"
#include "rect3.h"
#include "sound.h"

static bool ellipsoid_intersects_plane(const struct Ellipsoid *el, struct Plane pl)
{
	// TODO: this works, but rewrite with plane_move() so it makes more sense
	Vec3 center = mat3_mul_vec3(el->world2uball, el->center);
	plane_apply_mat3_INVERSE(&pl, el->uball2world);

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

static SDL_Rect bbox_without_hidelowerhalf(
	const struct Ellipsoid *el, const struct Camera *cam)
{
	Mat3 uball2cam = mat3_mul_mat3(cam->world2cam, el->uball2world);

	/*
	Each y coordinate on screen corresponds with a plane y/z = yzr, where yzr is a constant.
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

	return (SDL_Rect){ xmin, ymin, xmax-xmin, ymax-ymin };
}

static SDL_Rect bbox_of_middle_circle(
	const struct Ellipsoid *el, const struct Camera *cam)
{
	/*
	Similar to bbox_without_hidelowerhalf.
	Doing this with the 2D circle in the middle (y=0 in unit ball coords)
	basically leads to the same equations, but in 2D.
	*/
	Mat3 uball2world = el->uball2world;
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

	return (SDL_Rect){ xmin, ymin, xmax-xmin, ymax-ymin };
}

SDL_Rect ellipsoid_bbox(const struct Ellipsoid *el, const struct Camera *cam)
{
	SDL_Rect bbox = bbox_without_hidelowerhalf(el, cam);
	if (el->hidelowerhalf) {
		SDL_Rect circlebbox = bbox_of_middle_circle(el, cam);
		bbox.h = (circlebbox.y - bbox.y) + circlebbox.h;
	}

	SDL_Rect res;
	SDL_IntersectRect(&(SDL_Rect){ 0, 0, cam->surface->w, cam->surface->h }, &bbox, &res);
	SDL_assert(0 <= res.x && res.x+res.w <= cam->surface->w && 0 <= res.y && res.y+res.h <= cam->surface->h);
	return res;
}

struct Rect3 ellipsoid_get_sort_rect(const struct Ellipsoid *el, const struct Camera *cam)
{
	Vec3 center2cam = vec3_sub(cam->location, el->center);
	Vec3 dir = { center2cam.z, 0, -center2cam.x };
	Vec3 center2edge = vec3_withlength(dir, el->xzradius);

	Vec3 topleft, topright, botleft, botright;
	topleft = botleft = vec3_sub(el->center, center2edge);
	topright = botright = vec3_add(el->center, center2edge);
	topleft.y += el->yradius;
	topright.y += el->yradius;
	botleft.y -= el->yradius;
	botright.y -= el->yradius;

	if (el->hidelowerhalf) {
		// Hack to help stacking guards on top of player and other guards
		Vec3 tilt = vec3_withlength(center2cam, 0.01f);
		vec3_add_inplace(&botleft, tilt);
		vec3_add_inplace(&botright, tilt);
	}

	return (struct Rect3){.corners={ topleft, topright, botright, botleft }};
}

static void get_middle_circle_xzr_minmax(const struct Ellipsoid *el, const struct Camera *cam, int y, float *xzrmin, float *xzrmax)
{
	float yzr = camera_screeny_to_yzr(cam, y);

	// Find intersection of y/z = yzr (camera coords) and y=0 (unit ball coords)
	struct Plane yzrplane = { .normal = {0,1,-yzr}, .constant = 0 };
	plane_apply_mat3_INVERSE(&yzrplane, cam->world2cam);
	plane_move(&yzrplane, vec3_sub(el->center, cam->location));
	plane_apply_mat3_INVERSE(&yzrplane, el->uball2world);

	// Equation of yzrplane: (x,y,z) dot (a,b,c) = k
	float a = yzrplane.normal.x;
	float c = yzrplane.normal.z;
	float k = yzrplane.constant;

	// Intersecting yzrplane with y=0 gives a line (x,y,z) = p + t*dir
	float inv = 1/(a*a + c*c);
	Vec3 p = { a*k*inv, 0, c*k*inv };
	Vec3 dir = { -c, 0, a };

	// Solve t so that intersection is on the unit ball x^2+y^2+z^2=1
	float tSQUARED = inv - k*k*inv*inv;
	SDL_assert(tSQUARED >= 0);
	float t = sqrtf(tSQUARED);

	Vec3 p1 = vec3_add(p, vec3_mul_float(dir, -t));
	Vec3 p2 = vec3_add(p, vec3_mul_float(dir, t));
	vec3_apply_matrix(&p1, el->uball2world);
	vec3_apply_matrix(&p2, el->uball2world);
	vec3_add_inplace(&p1, vec3_sub(cam->location, el->center));
	vec3_add_inplace(&p2, vec3_sub(cam->location, el->center));
	vec3_apply_matrix(&p1, cam->world2cam);
	vec3_apply_matrix(&p2, cam->world2cam);
	*xzrmin = p1.x / p1.z;
	*xzrmax = p2.x / p2.z;
}

bool ellipsoid_xminmax(const struct Ellipsoid *el, const struct Camera *cam, int y, int *xmin, int *xmax)
{
	/*
	Consider the line that is t*(xzr,yzr,1) in camera coordinates.
	In unit ball coordinates, it will be

		t*(xzr*v + w) + p,

	where v, w and p don't depend on xzr or t.
	*/
	Mat3 world2uball = el->world2uball;
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

	if (el->hidelowerhalf) {
		// Find y coords of corresponding points on the unit ball (l left side, r right side)
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
#define ARRAY(T, Name) T Name[CAMERA_SCREEN_WIDTH]; LOOP Name[i]

	ARRAY(float, xzr) = camera_screenx_to_xzr(cam, (float)(xmin + i));

	/*
	line equation in camera coordinates:

		x = xzr*z, y = yzr*z aka (x,y,z) = z*(xzr,yzr,1)

	Note that the direction vector (xzr,yzr,1) is pointing towards the camera.
	*/
	float yzr = camera_screeny_to_yzr(cam, y);

	// Line equation in unit ball coordinates:  (x,y,z) = camloc + t*linedir
	Vec3 camloc = mat3_mul_vec3(el->world2uball, vec3_sub(cam->location, el->center));
	Mat3 M = mat3_mul_mat3(el->world2uball, cam->cam2world);
	ARRAY(float, linedirx) = mat3_mul_vec3(M, (Vec3){xzr[i],yzr,1}).x;
	ARRAY(float, linediry) = mat3_mul_vec3(M, (Vec3){xzr[i],yzr,1}).y;
	ARRAY(float, linedirz) = mat3_mul_vec3(M, (Vec3){xzr[i],yzr,1}).z;

	/*
	Intersecting the ball

		(x,y,z) dot (x,y,z) = 1

	with the line

		(x,y,z) = camloc + t*linedir

	creates a quadratic equation in t. We want the solution with bigger t,
	because the direction vector is pointing towards the camera.
	*/
	float cc = vec3_dot(camloc, camloc);
#define LineDir(i) ( (Vec3){ linedirx[i], linediry[i], linedirz[i] } )
	ARRAY(float, dd) = vec3_dot(LineDir(i), LineDir(i));
	ARRAY(float, cd) = vec3_dot(camloc, LineDir(i));
#undef LineDir

	ARRAY(float, tmp) = cd[i]*cd[i] - dd[i]*(cc-1);
	ARRAY(float, tmppos) = max(0, tmp[i]);   // no negative under sqrt plz. Don't know why t[i] can be more than just a little bit negative...
	ARRAY(float, roots) = sqrtf(tmppos[i]);
	ARRAY(float, t) = (roots[i] - cd[i])/dd[i];

	ARRAY(float, vecx) = linedirx[i]*t[i] + camloc.x;
	ARRAY(float, vecy) = linediry[i]*t[i] + camloc.y;
	ARRAY(float, vecz) = linedirz[i]*t[i] + camloc.z;

	ARRAY(int, ex) = (int)(ELLIPSOIDPIC_SIDE/2 * (1+vecx[i]));
	ARRAY(int, ey) = (int)(ELLIPSOIDPIC_SIDE/2 * (1+vecy[i]));
	ARRAY(int, ez) = (int)(ELLIPSOIDPIC_SIDE/2 * (1+vecz[i]));

	// just in case floats do something weird, e.g. division by zero
	LOOP clamp(&ex[i], 0, ELLIPSOIDPIC_SIDE-1);
	LOOP clamp(&ey[i], 0, ELLIPSOIDPIC_SIDE-1);
	LOOP clamp(&ez[i], 0, ELLIPSOIDPIC_SIDE-1);

	uint32_t *px = (uint32_t *)cam->surface->pixels + mypitch*y + xmin;
	bool hl = el->highlighted;
	LOOP px[i] = el->epic->cubepixels[hl][ex[i]][ey[i]][ez[i]];
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
	el->uball2world = mat3_mul_mat3(
		diag(el->xzradius, el->yradius, el->xzradius),
		mat3_rotation_xz(el->angle));
	el->world2uball = mat3_inverse(el->uball2world);
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

void ellipsoid_beginjump(struct Ellipsoid *el)
{
	SDL_assert(!el->jumpstate.jumping);
	log_printf("Jumper-jump begins");
	el->jumpstate.jumping = true;

	float v = el->jumpstate.xzspeed;
	el->jumpstate.speed = (Vec3){ v*sinf(el->angle), 30, -v*cosf(el->angle) };

	sound_play("superboing.wav");
}

static int clamp_with_bounce(float *val, float lo, float hi)
{
	int ret = 1;
	*val -= lo;
	if (*val < 0) {
		*val = -*val;
		ret = -1;
	}
	*val += lo;

	*val -= hi;
	if (*val > 0) {
		*val = -*val;
		ret = -1;
	}
	*val += hi;

	return ret;
}

void ellipsoid_jumping_eachframe(struct Ellipsoid *el, const struct Map *map)
{
	SDL_assert(el->jumpstate.jumping);
	vec3_add_inplace(&el->center, vec3_mul_float(el->jumpstate.speed, 1.0f/CAMERA_FPS));

	el->jumpstate.speed.x *= clamp_with_bounce(&el->center.x, el->xzradius, map->xsize - el->xzradius);
	el->jumpstate.speed.y -= GRAVITY/CAMERA_FPS;
	el->jumpstate.speed.z *= clamp_with_bounce(&el->center.z, el->xzradius, map->zsize - el->xzradius);

	float centerymin = el->hidelowerhalf ? 0 : el->yradius;
	if (el->jumpstate.speed.y < 0 && el->center.y < centerymin) {
		log_printf("end jump");
		el->center.y = centerymin;
		el->jumpstate.jumping = false;
	}
}
