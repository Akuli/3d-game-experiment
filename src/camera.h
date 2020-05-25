#ifndef CAMERA_H
#define CAMERA_H

#include <SDL2/SDL.h>
#include "vecmat.h"

/*
To convert a vector into coordinates where camera points to negative z direction,
subtract camera.location and then multiply by word2camera matrix.
*/
struct Camera {
	struct Vec3 location;
	struct Mat3 world2cam;
	SDL_Surface *surface;
};

/*
It's usually handy to have the camera at (0,0,0) pointing to negative z direction.
Coordinates like that are called "camera coordinates", and the usual coordinates
with camera being wherever it is are "world coordinates".
*/
struct Vec3 camera_point_world2cam(const struct Camera *cam, struct Vec3 v);
struct Vec3 camera_point_cam2world(const struct Camera *cam, struct Vec3 v);
struct Plane camera_plane_world2cam(const struct Camera *cam, struct Plane pl);
struct Plane camera_plane_cam2world(const struct Camera *cam, struct Plane pl);


#endif   // CAMERA_H
