#ifndef WALL_H
#define WALL_H

#include <stdbool.h>
#include "ellipsoid.h"
#include "camera.h"
#include "mathstuff.h"
#include "rect.h"

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

	       /top2
	      / |
	     /  |
	    /   |
	   /    |
	 top1   |
	  |     bot2
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

// Call this after setting startx,startz,dir of a new wall
// Can be called multiple times
void wall_init(struct Wall *w);

struct Rect wall_to_rect(const struct Wall *w);

// does not require using wall_init()
bool wall_match(const struct Wall *w1, const struct Wall *w2);

// moves el so that it doesn't bump
void wall_bumps_ellipsoid(const struct Wall *w, struct Ellipsoid *el);

// center point of wall in world coordinates
Vec3 wall_center(const struct Wall *w);

// same for any two points on same side of the wall
bool wall_side(const struct Wall *w, Vec3 pt);

// two walls are lined up if they are parallel and on the same plane
inline bool wall_linedup(const struct Wall *w1, const struct Wall *w2)
{
	return
		(w1->dir == WALL_DIR_XY && w2->dir == WALL_DIR_XY && w1->startz == w2->startz) ||
		(w1->dir == WALL_DIR_ZY && w2->dir == WALL_DIR_ZY && w1->startx == w2->startx);
}


#endif    // WALL_H
