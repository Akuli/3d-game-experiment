#include "showall.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include "ellipsoid.h"
#include "interval.h"
#include "player.h"
#include "mathstuff.h"


static inline float linear_map(float srcmin, float srcmax, float dstmin, float dstmax, float val)
{
	// ratio should get inlined when everything except val is constants
	float ratio = (dstmax - dstmin)/(srcmax - srcmin);
	return dstmin + (val - srcmin)*ratio;
}


// set to something else than 1 to make things fast but pixely
#define RAY_SKIP 1

struct VisibleEnemyInfo {
	const struct Enemy *enemy;
	Vec3 center;    // in camera coordinates with cam2uball applied
};

static int compare_visible_enemy_infos(const void *aptr, const void *bptr)
{
	const struct VisibleEnemyInfo *a = aptr, *b = bptr;
	return (a->center.z > b->center.z) - (a->center.z < b->center.z);
}

static size_t create_visible_enemy_infos(
	const struct Enemy *ens, size_t nens,
	struct VisibleEnemyInfo *res,
	const struct Camera *cam, Mat3 cam2uball)
{
	size_t n = 0;
	for (size_t i = 0; i < nens; i++) {
		if (ellipsoid_visible(&ens[i].ellipsoid, cam)) {
			res[n++] = (struct VisibleEnemyInfo){
				.enemy = &ens[i],
				.center = mat3_mul_vec3(cam2uball, camera_point_world2cam(cam, ens[i].ellipsoid.center)),
			};
		}
	}
	return n;
}

struct PlaneBallIntersectionCircle {
	Vec3 center;
	float radiusSQUARED;
};

static void calculate_y_minmax(
	int *ymin, int *ymax,
	struct PlaneBallIntersectionCircle ic,
	struct Plane xplane,
	const struct Camera *cam,
	Mat3 uball2cam)
{
	// camera is at (0,0,0)

	/*
	Intersection circle looks like this:

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
		     (0,0,0) = camera

	The \ and / lines are tangent lines of the intersection circle going
	through the camera. We want to find those, because they correspond to
	the first and last y pixel of the ball visible to the camera. Letting
	A and B denote the intersections of the tangent lines with the circle,
	and letting C denote the center of line AB, we see that the triangles

		B-C-icenter  and  camera-B-icenter

	are similar (90deg angle and the angle at icenter in both). Hence

		|C - icenter| / |icenter - B| = |B - icenter| / |icenter - camera|

	which gives

		|C - icenter| = radius^2 / |icenter|.

	The unit vector in direction of icenter (thinking of icenter as a
	vector from camera=(0,0,0) to icenter) is icenter/|icenter|, so

		C = icenter/|icenter| * (|icenter| - radius^2/|icenter|)
		  = icenter * (1 - radius^2/|icenter|^2)
	*/
	Vec3 C = vec3_mul_float(ic.center, 1 - ic.radiusSQUARED/vec3_lengthSQUARED(ic.center));

	/*
	Pythagorean theorem: radius^2 = |C-A|^2 + |C - icenter|^2

	Plugging in from above gives this. Note that it has radius^4, aka
	radius^2 * radius^2.
	*/
	float distanceCA = sqrtf(ic.radiusSQUARED - ic.radiusSQUARED*ic.radiusSQUARED/vec3_lengthSQUARED(ic.center));

	/*
	Use right hand rule for cross product, with xplane normal vector
	pointing from you towards your screen in above picture.

	The pointing direction could also be the opposite and you would get
	CtoB instead of CtoA. Doesn't matter.
	*/
	Vec3 CtoA = vec3_withlength(vec3_cross(ic.center, xplane.normal), distanceCA);
	Vec3 A = vec3_add(C, CtoA);
	Vec3 B = vec3_sub(C, CtoA);

	// Trial and error has been used to figure out which y value is bigger and which is smaller...
	*ymax = (int)camera_point_cam2screen(cam, mat3_mul_vec3(uball2cam, A)).y;
	*ymin = (int)camera_point_cam2screen(cam, mat3_mul_vec3(uball2cam, B)).y;
	assert(*ymin <= *ymax);

	if (*ymin < 0)
		*ymin = 0;
	if (*ymax > cam->surface->h)
		*ymax = cam->surface->h;
}

static uint32_t get_ellipsoid_color(
	const struct Ellipsoid *el, const struct Camera *cam,
	Mat3 rotation,
	float xzr, int y,
	Mat3 cam2unitb, Vec3 ballcenter)
{
	float yzr = camera_screeny_to_yzr(cam, (float)y);

	/*
	line equation in camera coordinates:

		x = xzr*z, y = yzr*z aka (x,y,z) = z*(xzr,yzr,1)

	Note that this has z coordinate 1 in camera coordinates, i.e. pointing toward camera
	*/
	Vec3 linedir = mat3_mul_vec3(cam2unitb, (Vec3){xzr,yzr,1});

	/*
	Let xyz denote the vector (x,y,z). Intersecting the ball

		(xyz - ballcenter) dot (xyz - ballcenter) = 1

	with the line

		xyz = t*linedir

	creates a quadratic equation in t. We want the solution with bigger t,
	because the direction vector is pointing towards the camera.
	*/
	float cc = vec3_lengthSQUARED(ballcenter);
	float dd = vec3_lengthSQUARED(linedir);
	float cd = vec3_dot(ballcenter, linedir);

	float undersqrt = cd*cd - dd*(cc-1);
	if (undersqrt < 0) {
		//log_printf("negative under sqrt: %f", undersqrt);
		undersqrt = 0;
	}

	float t = (cd + sqrtf(undersqrt))/dd;
	Vec3 vec = mat3_mul_vec3(rotation, vec3_sub(vec3_mul_float(linedir, t), ballcenter));

	int ex = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE-1, vec.x);
	int ey = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE-1, vec.y);
	int ez = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE-1, vec.z);

	if (ex < 0) ex = 0;
	if (ey < 0) ey = 0;
	if (ez < 0) ez = 0;

	if (ex >= ELLIPSOIDPIC_SIDE) ex = ELLIPSOIDPIC_SIDE-1;
	if (ey >= ELLIPSOIDPIC_SIDE) ey = ELLIPSOIDPIC_SIDE-1;
	if (ez >= ELLIPSOIDPIC_SIDE) ez = ELLIPSOIDPIC_SIDE-1;

	return el->epic->cubepixels[ex][ey][ez];
}

// about 2x faster than SDL_FillRect(surf, &(SDL_Rect){x,y,1,1}, px)
static inline void set_pixel(SDL_Surface *surf, int x, int y, uint32_t px)
{
	unsigned char *ptr = surf->pixels;
	ptr += y*surf->pitch;
	ptr += x*(int)sizeof(px);
	memcpy(ptr, &px, sizeof(px));   // no strict aliasing issues thx
}

void show_all(
	const struct Wall *walls, size_t nwalls,
	const struct Player *plrs, size_t nplrs,
	const struct Enemy *ens, size_t nens,
	const struct Camera *cam)
{
	assert(nens <= SHOWALL_MAX_ENEMIES);

	// cam2uball flattens any enemy from camera coordinates into unit ball
	Mat3 uball2cam = { .rows = {
		{ ENEMY_XZRADIUS, 0, 0 },
		{ 0, ENEMY_YRADIUS, 0 },
		{ 0, 0, ENEMY_XZRADIUS },
	}};
	Mat3 cam2uball = mat3_inverse(uball2cam);

	// static to keep stack usage down
	static struct VisibleEnemyInfo visens[SHOWALL_MAX_ENEMIES];
	size_t nvisens = create_visible_enemy_infos(ens, nens, visens, cam, cam2uball);
	qsort(visens, nvisens, sizeof(visens[0]), compare_visible_enemy_infos);

	for (int x = 0; x < cam->surface->w; x += RAY_SKIP) {
		float xzr = camera_screenx_to_xzr(cam, (float)x);

		// Equation of plane of points having this screen x:
		// x/z = xzr, aka 1x + 0y + (-xzr)z = 0
		struct Plane xplane = { .normal = {1, 0, -xzr}, .constant = 0 };
		plane_apply_mat3_INVERSE(&xplane, cam2uball);

		// Intersection of xplane with each enemy is a circle
		static struct PlaneBallIntersectionCircle icircles[SHOWALL_MAX_ENEMIES];

		static struct Interval intervals[SHOWALL_MAX_ENEMIES];
		size_t nintervals = 0;

		for (int v = 0; v < (int)nvisens; v++) {
			float dSQUARED = plane_point_distanceSQUARED(xplane, visens[v].center);
			if (dSQUARED >= 1)
				continue;

			icircles[v].radiusSQUARED = 1 - dSQUARED;   // Pythagorean theorem
			icircles[v].center = vec3_add(visens[v].center, vec3_withlength(xplane.normal, sqrtf(dSQUARED)));

			int ymin, ymax;
			calculate_y_minmax(&ymin, &ymax, icircles[v], xplane, cam, uball2cam);
			if (ymin < ymax) {
				intervals[nintervals++] = (struct Interval){
					.start = ymin,
					.end = ymax,
					.id = v,
				};
			}
		}

		static struct Interval nonoverlap[INTERVAL_NON_OVERLAPPING_MAX(SHOWALL_MAX_ENEMIES)];
		size_t nnonoverlap = interval_non_overlapping(intervals, nintervals, nonoverlap);

		for (size_t i = 0; i < nnonoverlap; i++) {
			struct VisibleEnemyInfo visen = visens[nonoverlap[i].id];

			Mat3 rot = mat3_rotation_xz(cam->angle - visen.enemy->ellipsoid.angle);

			for (int y = nonoverlap[i].start; y < nonoverlap[i].end; y++) {
				uint32_t col = get_ellipsoid_color(
					&visen.enemy->ellipsoid, cam, rot, xzr, y, cam2uball, visen.center);
				set_pixel(cam->surface, x, y, col);
			}
		}
	}
}
