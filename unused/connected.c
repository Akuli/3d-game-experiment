#include "editplace.h"
#include "log.h"
#include "misc.h"
#include "place.h"
#include "wall.h"
#include "looptimer.h"
#include "showall.h"
#include "camera.h"

enum ConnectedRegionFlags {
	CRF_WALL_XPOS = 0x01,
	CRF_WALL_XNEG = 0x02,
	CRF_WALL_ZPOS = 0x04,
	CRF_WALL_ZNEG = 0x08,
	CRF_VISITED = 0x10,
};

static void prepare_flags(const struct Place *pl, enum ConnectedRegionFlags *flags)
{
	for (int i = 0; i < pl->nwalls; i++) {
		struct Wall w = pl->walls[i];
		switch(w.dir) {
			case WALL_DIR_XY:
				// Careful with walls at edges
				if (w.startz != pl->zsize)
					flags[w.startz*pl->xsize + w.startx] |= CRF_WALL_ZNEG;
				if (w.startz != 0)
					flags[(w.startz-1)*pl->xsize + w.startx] |= CRF_WALL_ZPOS;
				break;
			case WALL_DIR_ZY:
				if (w.startx != pl->xsize)
					flags[w.startz*pl->xsize + w.startx] |= CRF_WALL_XNEG;
				if (w.startx != 0)
					flags[w.startz*pl->xsize + (w.startx-1)] |= CRF_WALL_XPOS;
				break;
		}
	}
}

static bool find_unnumbered_square(const struct Place *pl, const short *nums, int *xp, int *zp)
{
	for (int x = 0; x < pl->xsize; x++) {
		for (int z = 0; z < pl->zsize; z++) {
			if (nums[pl->xsize*z + x] == 0) {
				*xp = x;
				*zp = z;
				return true;
			}
		}
	}
	return false;
}

static void set_region_to_number(const struct Place *pl, enum ConnectedRegionFlags *flags, short *nums, short n, int initx, int initz)
{
	nums[pl->xsize*initz + initx] = n;
	bool ready;
	do {
		ready = true;
		for (int x = 0; x < pl->xsize; x++) {
			for (int z = 0; z < pl->zsize; z++) {
				enum ConnectedRegionFlags *f = &flags[z*pl->xsize + x];
				if (nums[pl->xsize*z + x] == n && !(*f & CRF_VISITED)) {
					// Mark neighbors as reachable too
					*f |= CRF_VISITED;
					if (!(*f & CRF_WALL_XNEG) && x != 0)
						nums[pl->xsize*z + (x-1)] = n;
					if (!(*f & CRF_WALL_XPOS) && x != pl->xsize-1)
						nums[pl->xsize*z + (x+1)] = n;
					if (!(*f & CRF_WALL_ZNEG) && z != 0)
						nums[pl->xsize*(z-1) + x] = n;
					if (!(*f & CRF_WALL_ZPOS) && z != pl->zsize-1)
						nums[pl->xsize*(z+1) + x] = n;
					ready = false;
				}
			}
		}
	} while (!ready);

	for (int x = 0; x < pl->xsize; x++)
		for (int z = 0; z < pl->zsize; z++)
			flags[z*pl->xsize + x] &= ~CRF_VISITED;
}

static short *number_connected_regions(const struct Place *pl)
{
	enum ConnectedRegionFlags *flags = calloc(pl->xsize*pl->zsize, sizeof flags[0]);
	short *nums = calloc(pl->xsize*pl->zsize, sizeof nums[0]);
	if (!flags || !nums)
		log_printf_abort("not enough memory");
	prepare_flags(pl, flags);

	short numcounter = 0;
	int x = pl->enemyloc.x;
	int z = pl->enemyloc.z;
	do {
		set_region_to_number(pl, flags, nums, ++numcounter, x, z);
	} while(find_unnumbered_square(pl, nums, &x, &z));

	free(flags);
	return nums;
}
