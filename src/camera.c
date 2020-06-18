#include "camera.h"
#include <assert.h>
#include <SDL2/SDL.h>
#include "mathstuff.h"

// non-static inlines are weird in c
extern inline float camera_xzr_to_screenx(const struct Camera *cam, float xzr);
extern inline float camera_yzr_to_screeny(const struct Camera *cam, float yzr);
extern inline float camera_screenx_to_xzr(const struct Camera *cam, float screenx);
extern inline float camera_screeny_to_yzr(const struct Camera *cam, float screeny);
extern inline Vec2 camera_point_cam2screen(const struct Camera *cam, Vec3 pt);
extern inline Vec3 camera_point_world2cam(const struct Camera *cam, Vec3 v);
extern inline Vec3 camera_point_cam2world(const struct Camera *cam, Vec3 v);

void camera_update_caches(struct Camera *cam)
{
	cam->cam2world = mat3_rotation_xz(cam->angle);
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

SDL_Surface *camera_create_cropped_surface(SDL_Surface *surf, SDL_Rect r)
{
	// TODO: use surf->format somehow?
	SDL_Surface *res = SDL_CreateRGBSurfaceFrom(
		(char*)surf->pixels + r.y*surf->pitch + sizeof(uint32_t)*r.x,
		r.w, r.h,
		32, surf->pitch, 0, 0, 0, 0);
	if (!res)
		log_printf_abort("SDL_CreateRGBSurfaceFrom failed: %s", SDL_GetError());
	return res;
}
