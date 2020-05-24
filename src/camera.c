#include "camera.h"
#include "vecmat.h"

struct Vec3 camera_point_world2cam(const struct Camera *cam, struct Vec3 v)
{
	// can be optimized by caching inverse matrix if needed
	return mat3_mul_vec3(cam->world2cam, vec3_sub(v, cam->location));
}

struct Vec3 camera_point_cam2world(const struct Camera *cam, struct Vec3 v)
{
	return vec3_add(mat3_mul_vec3(mat3_inverse(cam->world2cam), v), cam->location);
}

struct Plane camera_plane_cam2world(const struct Camera *cam, struct Plane pl)
{
	plane_apply_mat3_INVERSE(&pl, cam->world2cam);
	plane_move(&pl, cam->location);
	return pl;
}
