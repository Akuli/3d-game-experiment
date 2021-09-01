#ifndef WALL_H
#define WALL_H

#include <stdbool.h>
#include "camera.h"
#include "mathstuff.h"

#define WALL_Y_MIN 0.1f
#define WALL_Y_MAX 1.0f

/*
Walls always start and end in integer x and z coordinates and go 1 unit to
x or z direction from there, as specified by this.
*/
enum WallDirection { WALL_DIR_XY, WALL_DIR_ZY };

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
};

// Call this after setting startx,startz,dir of a new wall
// Can be called multiple times
void wall_init(struct Wall *w);

// does not require using wall_init()
bool wall_match(const struct Wall *w1, const struct Wall *w2);

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

void wall_drawborder(const struct Wall *w, const struct Camera *cam);

// for non-border drawing functions
struct WallCache {
	const struct Wall *wall;
	const struct Camera *cam;
	Vec2 top1, top2, bot1, bot2;  // screen points
};

/*
Returns whether wall is visible.

Many things in one function, hard to separate. Cache is needed for visibility checking,
but can't be created if not visible. Visibility checking also produces xmin and xmax.
*/
bool wall_visible_xminmax_fillcache(
	const struct Wall *w, const struct Camera *cam,
	int *xmin, int *xmax,   // If returns true, set to where on screen will wall go
	struct WallCache *wc    // Filled if returns true
);

// Which range of screen y coordinates is showing the wall?
void wall_yminmax(const struct WallCache *wc, int x, int *ymin, int *ymax);

// Draw all pixels of wall corresponding to range of y coordinates
void wall_drawcolumn(const struct WallCache *wc, int x, int ymin, int ymax, bool highlight);


#endif    // WALL_H
