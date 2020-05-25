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

SDL_Point camera_point_to_sdl(const struct Camera *cam, Vec3 pt)
{
	assert(pt.z < 0);
	return (SDL_Point){
		.x = (int) roundf(camera_xzr_to_screenx(cam, pt.x/pt.z)),
		.y = (int) roundf(camera_yzr_to_screeny(cam, pt.y/pt.z)),
	};
}

static void update_minmax(float *min, float *max, float val)
{
	if (val < *min)
		*min = val;
	if (val > *max)
		*max = val;
}

bool camera_containing_rect(const struct Camera *cam, const Vec3 *pts, size_t npts, SDL_Rect *res)
{
	assert(npts >= 1);

	// must be in front of the camera, i.e. negative z
	for (unsigned i = 0; i < npts; i++) {
		if (pts[i].z >= 0)
			return false;
	}

	float xmin, xmax, ymin, ymax;
	xmin = xmax = camera_xzr_to_screenx(cam, pts[0].x / pts[0].z);
	ymin = ymax = camera_yzr_to_screeny(cam, pts[0].y / pts[0].z);
	for (unsigned i = 1; i < npts; i++) {
		update_minmax(&xmin, &xmax, camera_xzr_to_screenx(cam, pts[i].x / pts[i].z));
		update_minmax(&ymin, &ymax, camera_yzr_to_screeny(cam, pts[i].y / pts[i].z));
	}

	res->x = (int) floorf(xmin);
	res->y = (int) floorf(ymin);
	res->w = (int) ceilf(xmax - xmin);
	res->h = (int) ceilf(ymax - ymin);
	return true;
}
