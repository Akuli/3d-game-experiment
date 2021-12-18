#ifndef WALL_H
#define WALL_H

#include <stdbool.h>
#include "ellipsoid.h"
#include "linalg.h"
#include "rect3.h"

/*
Walls always start and end in integer x and z coordinates and go 1 unit to
x or z direction from there, as specified by this.
*/
enum WallDirection { WALL_DIR_XY, WALL_DIR_ZY };

struct Wall {
	int startx;
	int startz;
	enum WallDirection dir;
};

// Can be called multiple times
void wall_init_collpoints(struct Wall *w);

struct Rect3 wall_to_rect(const struct Wall *w);

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
