#ifndef WALL_H
#define WALL_H

#include <stdbool.h>
#include <stdint.h>
#include "ellipsoid.h"
#include "camera.h"
#include "mathstuff.h"

/*
Walls always start and end in integer x and z coordinates and go 1 unit to
x or z direction from there, as specified by this.
*/
enum WallDirection { WALL_DIR_XY, WALL_DIR_ZY };

/*
I thought about doing collision checking by dividing it into these cases:
- The ellipsoid could touch the corner points of the wall.
- The ellipsoid could touch any edge of the wall so that it touches between the corners,
  and doesn't touch the corners.
- The ellipsoid could touch the "center part" of the wall without touching any edges or
  corners.

Handling all this would be a lot of code, so instead we just spread some points
uniformly across the wall and see if those touch. I call these collision points.
*/
#define WALL_CP_COUNT 10

struct Wall {
	int startx;
	int startz;
	enum WallDirection dir;

	/* corners in world coordinates, always up to date because walls don't move.

	now some 3D ascii art (imagine top1 and bot1 being closer to you)

           top2
	      / |
	     /  |
	    /   |
	   /    |
	 top1   |
	  |    bot2
	  |    /
	  |   /
	  |  /
	  | /
	 bot1

	top1 and bot1 aren't always closer to camera than top2 and bot2. The important
	thing is that top1 and bot1 are always vertically lined up, and so are top2
	and bot2.
	*/
	Vec3 top1, top2, bot1, bot2;

	// don't use outside wall.c
	Vec3 collpoints[WALL_CP_COUNT][WALL_CP_COUNT];
};

// Call this after setting startx, startz and dir of a new wall
void wall_init(struct Wall *w);

// moves el so that it doesn't bump
void wall_bumps_ellipsoid(const struct Wall *w, struct Ellipsoid *el);

// center point of wall in world coordinates
Vec3 wall_center(const struct Wall *w);

/*
Information shared in multiple functions
*/
struct WallCache {
	const struct Camera *cam;
	uint32_t color;

	// screen points
	Vec2 top1, top2, bot1, bot2;
};

/*
Returns whether wall is visible. If it is, values of xmin and xmax tell where on
the screen it will be shown and cache is filled.
*/
bool wall_visible_xminmax(
	const struct Wall *w, const struct Camera *cam, int *xmin, int *xmax, struct WallCache *wc);

// Which range of screen y coordinates is showing the wall?
void wall_yminmax(const struct WallCache *wc, int x, int *ymin, int *ymax);

/*
Is a point directly in front of or behind the wall? below pic is viewing from
above, and the return values of this function are denoted with T for true, and
F for false:

	F F T T T T T T T T T T F F
	F F T T T T T T T T T T F F
	F F T T T T T T T T T T F F
	F F T T =========== T T F F
	F F T T T T T T T T T T F F
	F F T T T T T T T T T T F F
	   |___|           |___|
	   offmax          offmax

Nothing is checked in y direction, aka "up/down" direction.
*/
bool wall_aligned_with_point(const struct Wall *w, Vec3 pt, float offmax);

// same for any two points on same side of the wall
bool wall_side(const struct Wall *w, Vec3 pt);

// two walls are lined up if they are parallel and on the same plane
bool wall_linedup(const struct Wall *w1, const struct Wall *w2);

/*
Draw all pixels of wall corresponding to range of y coordinates. May be
called more than once with same xcache but different ymin and ymax.
*/
void wall_drawcolumn(const struct WallCache *wc, int x, int ymin, int ymax);


#endif    // WALL_H
