#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "log.h"
#include "mathstuff.h"

/*
It's often handy to have the camera at (0,0,0) pointing to negative z direction.
Coordinates like that are called "camera coordinates", and the usual coordinates
with camera being wherever it is pointing to wherever it points to are "world
coordinates". Both coordinate systems are right-handed with y axis pointing up.
*/
struct Camera {
	SDL_Surface *surface;

	// call camera_update_caches() after changing these
	Vec3 location;
	Mat3 world2cam;

	/*
	For checking whether an object is visible or not, we split the world
	into visible and invisible parts with planes. The normal vector of each
	visibility plane points to the visible side. So, for a point to be
	visible, plane_whichside() must return true for each visibility plane.
	*/
	struct Plane visibilityplanes[5];
};

/*
The conversion between these consists of a rotation about the camera location and
offsetting by the camera location vector. This is what these functions do, but
there may be good reasons to not use these functions.
*/
Vec3 camera_point_world2cam(const struct Camera *cam, Vec3 v);
Vec3 camera_point_cam2world(const struct Camera *cam, Vec3 v);

/*
When mapping a point given in camera coordinates to a pixel on an SDL surface, we
only use the ratios point.z/point.x and point.z/point.y; we don't need to know
anything else about the point. I call these ratios xzr and yzr, short for "x to z
ratio" and "y to z ratio".
*/
float camera_screenx_to_xzr(const struct Camera *cam, float screenx);
float camera_screeny_to_yzr(const struct Camera *cam, float screeny);
float camera_xzr_to_screenx(const struct Camera *cam, float xzr);
float camera_yzr_to_screeny(const struct Camera *cam, float yzr);

// call this after changing cam->location or cam->world2cam
void camera_update_caches(struct Camera *cam);

/*
This asserts that pt.z is negative because otherwise the point is not in front
of the player.
*/
Vec2 camera_point_cam2screen(const struct Camera *cam, Vec3 pt);

/* Find the smallest rectangle containing the given 4 points

Why 4? Because hard-coding 4 turned out to be faster, and this needs to be fast.

All points are assumed to be in camera coordinates. This returns false if a point
is behind the camera. You must give at least one point.
*/
bool camera_get_containing_rect(
	const struct Camera *cam, SDL_Rect *res,
	Vec3 p1, Vec3 p2, Vec3 p3, Vec3 p4);


#endif   // CAMERA_H
