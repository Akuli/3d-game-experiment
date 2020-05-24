#include "vecmat.h"
#include <assert.h>
#include <math.h>

struct Vec3 vec3_add(struct Vec3 v, struct Vec3 w)
{
	return (struct Vec3){ v.x+w.x, v.y+w.y, v.z+w.z };
}

struct Vec3 vec3_neg(struct Vec3 v)
{
	return (struct Vec3){ -v.x, -v.y, -v.z };
}

struct Vec3 vec3_sub(struct Vec3 v, struct Vec3 w)
{
	return (struct Vec3){ v.x-w.x, v.y-w.y, v.z-w.z };
}

float vec3_dot(struct Vec3 v, struct Vec3 w)
{
	return v.x*w.x + v.y*w.y + v.z*w.z;
}

float vec3_lengthSQUARED(struct Vec3 v)
{
	return vec3_dot(v,v);
}

struct Vec3 vec3_withlength(struct Vec3 v, float len)
{
	float oldlen = sqrtf(vec3_lengthSQUARED(v));
	float r = len / oldlen;
	return (struct Vec3){ v.x*r, v.y*r, v.z*r };
}

struct Vec3 vec3_cross(struct Vec3 v, struct Vec3 w)
{
	/*
	| i j k |    | b c |    | a c |    | a b |
	| a b c | = i| e f | - j| d f | + k| d e |
	| d e f |
	          = (bf-ce)i - (af-cd)j + (ae-bd)k
	*/
	float a = v.x, b = v.y, c = v.z;
	float d = w.x, e = w.y, f = w.z;
	return (struct Vec3) {
		.x = b*f - c*e,
		.y = -(a*f - c*d),
		.z = a*e - b*d,
	};
}


const struct Mat3 mat3_id = { .rows = {
	{1,0,0},
	{0,1,0},
	{0,0,1},
}};

struct Vec3 mat3_mul_vec3(struct Mat3 M, struct Vec3 v)
{
	return (struct Vec3) {
		v.x*M.rows[0][0] + v.y*M.rows[0][1] + v.z*M.rows[0][2],
		v.x*M.rows[1][0] + v.y*M.rows[1][1] + v.z*M.rows[1][2],
		v.x*M.rows[2][0] + v.y*M.rows[2][1] + v.z*M.rows[2][2],
	};
}

struct Mat3 mat3_mul_float(struct Mat3 M, float f)
{
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			M.rows[i][j] *= f;   // pass-by-value

	return M;
}

float mat3_det(struct Mat3 M)
{
	struct Vec3 row1 = { M.rows[0][0], M.rows[0][1], M.rows[0][2] };
	struct Vec3 row2 = { M.rows[1][0], M.rows[1][1], M.rows[1][2] };
	struct Vec3 row3 = { M.rows[2][0], M.rows[2][1], M.rows[2][2] };
	return vec3_dot(row1, vec3_cross(row2, row3));
}

struct Mat3 mat3_inverse(struct Mat3 M)
{
	// https://ardoris.wordpress.com/2008/07/18/general-formula-for-the-inverse-of-a-3x3-matrix/
	float
		a=M.rows[0][0], b=M.rows[0][1], c=M.rows[0][2],
		d=M.rows[1][0], e=M.rows[1][1], f=M.rows[1][2],
		g=M.rows[2][0], h=M.rows[2][1], i=M.rows[2][2];

	return mat3_mul_float(
		(struct Mat3){ .rows = {
			{ e*i-f*h, c*h-b*i, f*b-c*e },
			{ f*g-d*i, a*i-c*g, c*d-a*f },
			{ d*h-e*g, b*g-a*h, a*e-b*d },
		}},
		1.0f/mat3_det(M)
	);
}

struct Mat3 mat3_rotation_xz(float angle)
{
	/*
	if you have understood 3blue1brown's linear transform stuff, then you should
	be able to write this down without looking it up
	*/
	return (struct Mat3){ .rows = {
		{ cosf(angle),  0, sinf(angle) },
		{ 0,            1, 0           },
		{ -sinf(angle), 0, cosf(angle) },
	}};
}


void plane_apply_mat3_INVERSE(struct Plane *pl, struct Mat3 inverse)
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
	*/
	pl->normal = mat3_mul_vec3(inverse, pl->normal);
}

void plane_move(struct Plane *pl, struct Vec3 mv)
{
	/*
	Generally, moving foo means replacing (x,y,z) with (x,y,z)-mv in the equation
	of foo (similarly to inverse matrix stuff above). Our plane equation is

		(x,y,z) dot normal = constant

	and we want to turn it into

		((x,y,z) - mv) dot normal = constant.

	This can be rewritten as

		(x,y,z) dot normal = constant + (mv dot normal).
	*/
	pl->constant += vec3_dot(mv, pl->normal);
}

bool plane_whichside(struct Plane pl, struct Vec3 pt)
{
	return (vec3_dot(pl.normal, pt) > pl.constant);
}

float plane_point_distance(struct Plane pl, struct Vec3 pt)
{
	/*
	3D version of this: https://akuli.github.io/math-derivations/analytic-plane-geometry/distance-line-point.html

	Constant has minus sign because it's written to different side of equation
	here than in the link.
	*/
	return fabsf(vec3_dot(pl.normal, pt) - pl.constant) / sqrtf(vec3_lengthSQUARED(pl.normal));
}
