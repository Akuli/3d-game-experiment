#include "showall.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include "ellipsoid.h"
#include "interval.h"
#include "player.h"
#include "mathstuff.h"


struct VisibleEllipsoidInfo {
	const struct Ellipsoid *el;
	struct EllipsoidXCache xcache;
	float camcenterz;   // center z in camera coordinates
};

static size_t create_visible_enemy_infos(
	const struct Enemy *ens, size_t nens,
	struct VisibleEllipsoidInfo *res,
	const struct Camera *cam)
{
	size_t n = 0;
	for (size_t i = 0; i < nens; i++) {
		if (ellipsoid_visible(&ens[i].ellipsoid, cam)) {
			res[n++] = (struct VisibleEllipsoidInfo){
				.el = &ens[i].ellipsoid,
				.camcenterz = camera_point_world2cam(cam, ens[i].ellipsoid.center).z,
			};
		}
	}
	return n;
}

static int compare_visible_enemy_infos(const void *aptr, const void *bptr)
{
	const struct VisibleEllipsoidInfo *a = aptr, *b = bptr;
	return (a->camcenterz > b->camcenterz) - (a->camcenterz < b->camcenterz);
}

void show_all(
	const struct Wall *walls, size_t nwalls,
	const struct Player *plrs, size_t nplrs,
	const struct Enemy *ens, size_t nens,
	const struct Camera *cam)
{
	assert(nens <= SHOWALL_MAX_ENEMIES);

	// static to keep stack usage down
	static struct VisibleEllipsoidInfo visels[SHOWALL_MAX_PLAYERS + SHOWALL_MAX_ENEMIES];
	size_t nvisels = create_visible_enemy_infos(ens, nens, visels, cam);
	qsort(visels, nvisels, sizeof(visels[0]), compare_visible_enemy_infos);

	for (int x = 0; x < cam->surface->w; x++) {
		static struct Interval intervals[SHOWALL_MAX_ENEMIES];
		size_t nintervals = 0;

		for (int v = 0; v < (int)nvisels; v++) {
			if (!ellipsoid_visible_x(visels[v].el, cam, x, &visels[v].xcache))
				continue;

			int ymin, ymax;
			ellipsoid_yminmax(visels[v].el, &visels[v].xcache, &ymin, &ymax);
			if (ymin < ymax) {
				intervals[nintervals++] = (struct Interval){
					.start = ymin,
					.end = ymax,
					.id = v,
				};
			}
		}

		static struct Interval nonoverlap[INTERVAL_NON_OVERLAPPING_MAX( sizeof(intervals)/sizeof(intervals[0]) )];
		size_t nnonoverlap = interval_non_overlapping(intervals, nintervals, nonoverlap);

		for (size_t i = 0; i < nnonoverlap; i++) {
			const struct VisibleEllipsoidInfo *visel = &visels[nonoverlap[i].id];
			ellipsoid_drawcolumn(visel->el, &visel->xcache, nonoverlap[i].start, nonoverlap[i].end);
		}
	}
}
