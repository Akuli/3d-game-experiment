#ifndef WALL_H
#define WALL_H

#include "ball.h"
#include "camera.h"
#include "mathstuff.h"

/*
Walls always start and end in integer x and z coordinates and go 1 unit to
x or z direction from there, as specified by this.
*/
enum WallDirection { WALL_DIR_XY, WALL_DIR_ZY };

/*
I thought about doing collision checking by dividing it into these cases:
- The ball could touch the corner points of the wall.
- The ball could touch any edge of the wall so that it touches between the corners,
  and doesn't touch the corners.
- The ball could touch the "center part" of the wall without touching any edges or
  corners.

Handling all this would be a lot of code, so instead we just spread some points
uniformly across the wall and see if those touch. I call these collision points.
*/
#define WALL_CP_COUNT 10

struct Wall {
	int startx;
	int startz;
	enum WallDirection dir;

	/*
	To check collision between a wall and a player, we need to switch to
	linear-transformed coordinates where the player is a ball (as in an actual
	ball, not a stretched ball).
	*/
	Vec3 collpoint_cache[WALL_CP_COUNT][WALL_CP_COUNT];
};

// Call this after creating a new Wall
void wall_initcaches(struct Wall *w);

// moves ball so that it doesn't bump
void wall_bumps_ball(const struct Wall *w, struct Ball *ball);

void wall_show(const struct Wall *w, const struct Camera *cam);

// center point of wall in world coordinates
Vec3 wall_center(const struct Wall *w);

// same for any two points on same side of the wall
bool wall_side(const struct Wall *wall, Vec3 pt);

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


#endif    // WALL_H
