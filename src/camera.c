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
	/*
	The plane equation can be written as ax+by+cz = constant. By thinking of
	numbers as 1x1 matrices, we can write that as

		         _   _
		        |  x  |
		[a b c] |  y  | = constant.
		        |_ z _|

	How to apply a matrix to the plane? Consider the two planes that we should
	have before and after applying the matrix. A point is on the plane after
	applying the transform if and only if the INVERSE transformed point is on the
	plane before applying the transform. This means that to apply a matrix M, we
	should replace the equation with

		              _   _
		             |  x  |
		[a b c] M^-1 |  y  | = constant,
		             |_ z _|

	and from linear algebra, we know that

		                       _   _    T
		               /      |  a  | \
		[a b c] M^-1 = | M^-1 |  b  | |
		               \      |_ c _| /

	In this case, M is a matrix taking vectors from camera coordinates to world
	coordinates, so M^-1 is the world2cam matrix.
	*/
	pl.normal = mat3_mul_vec3(cam->world2cam, pl.normal);
	plane_move(&pl, cam->location);
	return pl;
}
