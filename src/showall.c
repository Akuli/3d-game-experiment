#include "showall.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include "ellipsoid.h"
#include "interval.h"
#include "player.h"
#include "mathstuff.h"


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

static void calculate_y_minmax(
	int *ymin, int *ymax,
	float dSQUARED, Vec3 ballcenter, struct Plane xplane, const struct Camera *cam, Mat3 uball2cam)
{
	// camera is at (0,0,0)

	// The intersection is a circle
	float radiusSQUARED = 1 - dSQUARED;   // Pythagorean theorem
	Vec3 icenter = vec3_add(ballcenter, vec3_withlength(xplane.normal, sqrtf(dSQUARED)));

	/*
	On xplane, the situation looks like this:

		       o o
		    o       o
		  o           o
		\o   icenter   o/
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
	Vec3 C = vec3_mul_float(icenter, 1 - radiusSQUARED/vec3_lengthSQUARED(icenter));

	/*
	Pythagorean theorem: radius^2 = |C-A|^2 + |C - icenter|^2

	Plugging in from above gives this. Note that it has radius^4, aka
	radius^2 * radius^2.
	*/
	float distanceCA = sqrtf(radiusSQUARED - radiusSQUARED*radiusSQUARED/vec3_lengthSQUARED(icenter));

	/*
	Use right hand rule for cross product, with xplane normal vector
	pointing from you towards your screen in above picture.

	The pointing direction could also be the opposite and you would get
	CtoB instead of CtoA. Doesn't matter.
	*/
	Vec3 CtoA = vec3_withlength(vec3_cross(icenter, xplane.normal), distanceCA);
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

	uint32_t color = SDL_MapRGB(cam->surface->format, 0xff, 0, 0xff);

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

		static struct Interval intervals[SHOWALL_MAX_ENEMIES];
		size_t nintervals = 0;

		for (int v = 0; v < (int)nvisens; v++) {
			float dSQUARED = plane_point_distanceSQUARED(xplane, visens[v].center);
			if (dSQUARED >= 1)
				continue;

			int ymin, ymax;
			calculate_y_minmax(&ymin, &ymax, dSQUARED, visens[v].center, xplane, cam, uball2cam);
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
			SDL_FillRect(cam->surface, &(SDL_Rect){
				x, nonoverlap[i].start,
				RAY_SKIP, nonoverlap[i].end - nonoverlap[i].start,
			}, color);
		}
	}
}
