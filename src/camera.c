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

struct FPoint camera_point_cam2fpoint(const struct Camera *cam, Vec3 pt)
{
	assert(pt.z < 0);
	return (struct FPoint){
		.x = camera_xzr_to_screenx(cam, pt.x/pt.z),
		.y = camera_yzr_to_screeny(cam, pt.y/pt.z),
	};
}

#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))
#define min4(a,b,c,d) min(min(a,b),min(c,d))
#define max4(a,b,c,d) max(max(a,b),max(c,d))

bool camera_get_containing_rect(
	const struct Camera *cam, SDL_Rect *res,
	Vec3 p1, Vec3 p2, Vec3 p3, Vec3 p4)
{
	if (p1.z >= 0 || p2.z >= 0 || p3.z >= 0 || p4.z >= 0)
		return false;

	float x1 = camera_xzr_to_screenx(cam, p1.x / p1.z);
	float x2 = camera_xzr_to_screenx(cam, p2.x / p2.z);
	float x3 = camera_xzr_to_screenx(cam, p3.x / p3.z);
	float x4 = camera_xzr_to_screenx(cam, p4.x / p4.z);

	float y1 = camera_yzr_to_screeny(cam, p1.y / p1.z);
	float y2 = camera_yzr_to_screeny(cam, p2.y / p2.z);
	float y3 = camera_yzr_to_screeny(cam, p3.y / p3.z);
	float y4 = camera_yzr_to_screeny(cam, p4.y / p4.z);

	float xmin = min4(x1,x2,x3,x4);
	float xmax = max4(x1,x2,x3,x4);
	float ymin = min4(y1,y2,y3,y4);
	float ymax = max4(y1,y2,y3,y4);

	res->x = (int) xmin;
	res->y = (int) ymin;
	res->w = (int) (xmax - xmin + 1.);
	res->h = (int) (ymax - ymin + 1.);
	return true;
}
