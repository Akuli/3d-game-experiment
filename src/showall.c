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

static float calculate_center_y(
	struct Plane xplane,
	Vec3 ballcenter,
	Mat3 uball2cam,
	const struct Camera *cam)
{
	assert(xplane.constant == 0);  // goes through camera at (0,0,0)
	assert(xplane.normal.y == 0);  // not tilted
	assert(xplane.normal.x > 0);   // seems to be always true, needed to figure out which z solution is bigger

	/*
	We want to intersect these:
	- unit ball: (x - ballcenter.x)^2 + (y - ballcenter.y)^2 + (z - ballcenter.z)^2 = 1
	- plane that splits unit ball in half: y = ballcenter.y
	- xplane: x*xplane.normal.x + z*xplane.normal.z = 0

	Intersecting all that gives 3 equations with 3 unknowns x,y,z. We want the
	solution with bigger z because that's closer to the camera.
	*/

	float bottom = vec3_lengthSQUARED(xplane.normal);
	float dot = vec3_dot(xplane.normal, ballcenter);
	float undersqrt = bottom - dot*dot;
	if (undersqrt < 0) {
		printf("won't put under sqrt: %f\n", undersqrt);
		undersqrt = 0;
	}
	float infrontofsqrt = xplane.normal.x*ballcenter.z - ballcenter.x*xplane.normal.z;
	float tmp = (infrontofsqrt + sqrtf(undersqrt))/bottom;
	Vec3 v = { -xplane.normal.z*tmp, ballcenter.y, xplane.normal.x*tmp };

	vec3_apply_matrix(&v, uball2cam);
	return camera_yzr_to_screeny(cam, v.y/v.z);
}

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

// about 2x faster than SDL_FillRect(surf, &(SDL_Rect){x,y,1,1}, px)
static inline void set_pixel(SDL_Surface *surf, int x, int y, uint32_t px)
{
	unsigned char *ptr = surf->pixels;
	ptr += y*surf->pitch;
	ptr += x*(int)sizeof(px);
	memcpy(ptr, &px, sizeof(px));   // no strict aliasing issues thx
}

static void draw_ellipsoid_column(
	const struct Ellipsoid *el, const struct Camera *cam,
	Mat3 rotation,
	int x, float xzr,
	int ymin, int ydiff,
	Mat3 cam2unitb, Vec3 ballcenter)
{
	if (ydiff <= 0)
		return;
	if (ydiff > SHOWALL_SCREEN_HEIGHT)
		log_printf_abort("ydiff should be less than screen height but it seems insanely huge: %d", ydiff);

	/*
	Code is ugly but gcc vectorizes it to make it very fast. This code was the
	bottleneck of the game before making it more vectorizable, and it still is
	at the time of writing this comment.
	*/

#define LOOP for(int i = 0; i < ydiff; i++)
	float yzr[SHOWALL_SCREEN_HEIGHT];
	LOOP yzr[i] = camera_screeny_to_yzr(cam, (float)(ymin + i));

	/*
	line equation in camera coordinates:

		x = xzr*z, y = yzr*z aka (x,y,z) = z*(xzr,yzr,1)

	Note that this has z coordinate 1 in camera coordinates, i.e. pointing toward camera
	*/
	float linedirx[SHOWALL_SCREEN_HEIGHT], linediry[SHOWALL_SCREEN_HEIGHT], linedirz[SHOWALL_SCREEN_HEIGHT];
	LOOP linedirx[i] = mat3_mul_vec3(cam2unitb, (Vec3){xzr,yzr[i],1}).x;
	LOOP linediry[i] = mat3_mul_vec3(cam2unitb, (Vec3){xzr,yzr[i],1}).y;
	LOOP linedirz[i] = mat3_mul_vec3(cam2unitb, (Vec3){xzr,yzr[i],1}).z;
#define LineDir(i) ( (Vec3){ linedirx[i], linediry[i], linedirz[i] } )

	/*
	Let xyz denote the vector (x,y,z). Intersecting the ball

		(xyz - ballcenter) dot (xyz - ballcenter) = 1

	with the line

		xyz = t*linedir

	creates a quadratic equation in t. We want the solution with bigger t,
	because the direction vector is pointing towards the camera.
	*/
	float cc = vec3_lengthSQUARED(ballcenter);    // ballcenter dot ballcenter
	float dd[SHOWALL_SCREEN_HEIGHT];    // linedir dot linedir
	float cd[SHOWALL_SCREEN_HEIGHT];    // ballcenter dot linedir
	LOOP dd[i] = vec3_lengthSQUARED(LineDir(i));
	LOOP cd[i] = vec3_dot(ballcenter, LineDir(i));

	float t[SHOWALL_SCREEN_HEIGHT];
	LOOP t[i] = cd[i]*cd[i] - dd[i]*(cc-1);
	LOOP t[i] = max(0, t[i]);   // no negative under sqrt plz
	LOOP t[i] = (cd[i] + sqrtf(t[i]))/dd[i];

	float vecx[SHOWALL_SCREEN_HEIGHT], vecy[SHOWALL_SCREEN_HEIGHT], vecz[SHOWALL_SCREEN_HEIGHT];
	LOOP vecx[i] = mat3_mul_vec3(rotation, vec3_sub(vec3_mul_float(LineDir(i), t[i]), ballcenter)).x;
	LOOP vecy[i] = mat3_mul_vec3(rotation, vec3_sub(vec3_mul_float(LineDir(i), t[i]), ballcenter)).y;
	LOOP vecz[i] = mat3_mul_vec3(rotation, vec3_sub(vec3_mul_float(LineDir(i), t[i]), ballcenter)).z;
#undef LineDir

	int ex[SHOWALL_SCREEN_HEIGHT], ey[SHOWALL_SCREEN_HEIGHT], ez[SHOWALL_SCREEN_HEIGHT];
	LOOP ex[i] = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE-1, vecx[i]);
	LOOP ey[i] = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE-1, vecy[i]);
	LOOP ez[i] = (int)linear_map(-1, 1, 0, ELLIPSOIDPIC_SIDE-1, vecz[i]);

	LOOP ex[i] = max(0, min(ELLIPSOIDPIC_SIDE-1, ex[i]));
	LOOP ey[i] = max(0, min(ELLIPSOIDPIC_SIDE-1, ey[i]));
	LOOP ez[i] = max(0, min(ELLIPSOIDPIC_SIDE-1, ez[i]));

	uint32_t px[SHOWALL_SCREEN_HEIGHT];
	LOOP px[i] = el->epic->cubepixels[ex[i]][ey[i]][ez[i]];
	LOOP set_pixel(cam->surface, x, ymin + i, px[i]);
#undef LOOP
}

static int compare_visible_enemy_infos(const void *aptr, const void *bptr)
{
	const struct VisibleEnemyInfo *a = aptr, *b = bptr;
	return (a->center.z > b->center.z) - (a->center.z < b->center.z);
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
			if (visens[v].enemy->ellipsoid.epic->hidelowerhalf)
				ymax = calculate_center_y(xplane, visens[v].center, uball2cam, cam);

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
			int ymin = nonoverlap[i].start;
			int ydiff = nonoverlap[i].end - nonoverlap[i].start;
			draw_ellipsoid_column(
				&visen.enemy->ellipsoid, cam, rot,
				x, xzr,
				ymin, ydiff,
				cam2uball, visen.center);
		}
	}
}
