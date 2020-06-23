#ifndef WALL_H
#define WALL_H

#include "ball.h"
#include "camera.h"
#include "mathstuff.h"

/*
Walls always start and end in integer x and z coordinates
*/
#define WALL_HEIGHT 1.0f
enum WallDirection { WALL_DIR_X, WALL_DIR_Z };

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

// Return a plane that the wall is a part of
struct Plane wall_getplane(const struct Wall *w);

// Get all corners of the wall, in camera coordinates
void wall_getcamcorners(const struct Wall *w, const struct Camera *cam, Vec3 *res);

// same for any two points on same side of the wall
bool wall_side(const struct Wall *wall, Vec3 pt);

// Find intersection point of wall and line, return false if no intersection
bool wall_intersect_line(const struct Wall *w, struct Line ln, Vec3 *res);


#endif    // WALL_H
