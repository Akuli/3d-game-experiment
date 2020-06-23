#include "camera.h"
#include <assert.h>
#include <SDL2/SDL.h>
#include "mathstuff.h"

#define SCALING_FACTOR 300.f

Vec3 camera_point_world2cam(const struct Camera *cam, Vec3 v)
{
	// can be optimized by caching inverse matrix if needed
	return mat3_mul_vec3(cam->world2cam, vec3_sub(v, cam->location));
}

Vec3 camera_point_cam2world(const struct Camera *cam, Vec3 v)
{
	return vec3_add(mat3_mul_vec3(mat3_inverse(cam->world2cam), v), cam->location);
}

/*
Hints for figuring out the formulas:
- Usually z is negative, so xzr has the opposite sign as x, and yzr has the
  opposite sign of y.
- For x=0 and y=0, we want the center of the SDL surface.
- More x means right, which means means more screen x. More y means up, which
  means *less* screen y. That's how coordinates work in most 2D graphics things.
*/
float camera_xzr_to_screenx(const struct Camera *cam, float xzr) { return (float)cam->surface->w/2 - SCALING_FACTOR*xzr; }
float camera_yzr_to_screeny(const struct Camera *cam, float yzr) { return (float)cam->surface->h/2 + SCALING_FACTOR*yzr; }

float camera_screenx_to_xzr(const struct Camera *cam, float screenx) { return (-screenx + (float)cam->surface->w/2)/SCALING_FACTOR; }
float camera_screeny_to_yzr(const struct Camera *cam, float screeny) { return (screeny - (float)cam->surface->h/2)/SCALING_FACTOR; }

Vec2 camera_point_cam2screen(const struct Camera *cam, Vec3 pt)
{
	assert(pt.z < 0);
	return (Vec2){
		.x = camera_xzr_to_screenx(cam, pt.x/pt.z),
		.y = camera_yzr_to_screeny(cam, pt.y/pt.z),
	};
}

void camera_update_caches(struct Camera *cam)
{
	cam->world2cam = mat3_rotation_xz(-cam->angle);

	// see also CAMERA_CAMPLANE_IDX
	struct Plane pl[] = {
		// z=0, with normal vector to negative side (that's where camera is looking)
		{ .normal = {0, 0, -1}, .constant = 0 },

		// left side of view: x/z = xzr, aka 1x + 0y + (-xzr)z = 0, normal vector to positive x direction
		{ .normal = {1, 0, -camera_screenx_to_xzr(cam, 0)}, .constant = 0 },

		// right side of view, normal vector to negative x direction
		{ .normal = {-1, 0, camera_screenx_to_xzr(cam, (float)cam->surface->w)}, .constant = 0 },

		// top, normal vector to negative y direction
		{ .normal = {0, -1, camera_screeny_to_yzr(cam, 0)}, .constant = 0 },

		// bottom, normal vector to positive y direction
		{ .normal = {0, 1, -camera_screeny_to_yzr(cam, (float)cam->surface->h)}, .constant = 0 },
	};

	// Convert from camera coordinates to world coordinates
	for (int i = 0; i < sizeof(pl)/sizeof(pl[0]); i++) {
		plane_apply_mat3_INVERSE(&pl[i], cam->world2cam);
		plane_move(&pl[i], cam->location);
	}

	static_assert(sizeof(pl) == sizeof(cam->visplanes), "");
	memcpy(cam->visplanes, pl, sizeof(pl));
}
