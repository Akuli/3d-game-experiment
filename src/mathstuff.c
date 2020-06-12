#include "mathstuff.h"
#include <assert.h>
#include <math.h>

/*
Non-static inline functions are weird in c. You need to put definition to h file
and declaration to c file.
*/

inline Vec3 vec3_add(Vec3 v, Vec3 w);
inline Vec3 vec3_sub(Vec3 v, Vec3 w);
inline void vec3_add_inplace(Vec3 *v, Vec3 w);
inline void vec3_sub_inplace(Vec3 *v, Vec3 w);
inline Vec3 vec3_mul_float(Vec3 v, float f);
inline float vec3_dot(Vec3 v, Vec3 w);
inline float vec3_lengthSQUARED(Vec3 v);
inline Vec3 vec3_withlength(Vec3 v, float len);
inline Vec3 vec3_cross(Vec3 v, Vec3 w);
inline Vec3 mat3_mul_vec3(Mat3 M, Vec3 v);
inline void vec3_apply_matrix(Vec3 *v, Mat3 M);
inline bool plane_whichside(struct Plane pl, Vec3 pt);

Mat3 mat3_mul_mat3(Mat3 A, Mat3 B)
{
	Vec3 i = {1,0,0};
	Vec3 j = {0,1,0};
	Vec3 k = {0,0,1};

	// calculate columns in 3blue1brown style
	Vec3 col1 = mat3_mul_vec3(A, mat3_mul_vec3(B, i));
	Vec3 col2 = mat3_mul_vec3(A, mat3_mul_vec3(B, j));
	Vec3 col3 = mat3_mul_vec3(A, mat3_mul_vec3(B, k));

	return (Mat3){ .rows = {
		{ col1.x, col2.x, col3.x },
		{ col1.y, col2.y, col3.y },
		{ col1.z, col2.z, col3.z },
	}};
}

static Mat3 multiply_matrix_by_float(Mat3 M, float f)
{
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			M.rows[i][j] *= f;   // pass-by-value

	return M;
}

static float determinant(Mat3 M)
{
	Vec3 row1 = { M.rows[0][0], M.rows[0][1], M.rows[0][2] };
	Vec3 row2 = { M.rows[1][0], M.rows[1][1], M.rows[1][2] };
	Vec3 row3 = { M.rows[2][0], M.rows[2][1], M.rows[2][2] };
	return vec3_dot(row1, vec3_cross(row2, row3));
}

Mat3 mat3_inverse(Mat3 M)
{
	// https://ardoris.wordpress.com/2008/07/18/general-formula-for-the-inverse-of-a-3x3-matrix/
	float
		a=M.rows[0][0], b=M.rows[0][1], c=M.rows[0][2],
		d=M.rows[1][0], e=M.rows[1][1], f=M.rows[1][2],
		g=M.rows[2][0], h=M.rows[2][1], i=M.rows[2][2];

	return multiply_matrix_by_float(
		(Mat3){ .rows = {
			{ e*i-f*h, c*h-b*i, f*b-c*e },
			{ f*g-d*i, a*i-c*g, c*d-a*f },
			{ d*h-e*g, b*g-a*h, a*e-b*d },
		}},
		1.0f/determinant(M)
	);
}

Mat3 mat3_rotation_xz_sincos(float sin, float cos)
{
	/*
	if you have understood 3blue1brown's linear transform stuff and trig
	basics, then you should be able to write this without looking it up
	*/
	return (Mat3){ .rows = {
		{ cos, 0, -sin },
		{ 0,   1, 0    },
		{ sin, 0, cos  },
	}};
}

Mat3 mat3_rotation_xz(float angle)
{
	return mat3_rotation_xz_sincos(sinf(angle), cosf(angle));
}


static void swap(float *a, float *b)
{
	float tmp = *a;
	*a = *b;
	*b = tmp;
}

static void transpose(Mat3 *M)
{
	for (int i = 0; i < 3; i++)
		for (int k = 0; k < i; k++)
			swap(&M->rows[i][k], &M->rows[k][i]);
}

void plane_apply_mat3_INVERSE(struct Plane *pl, Mat3 inverse)
{
	/*
	The plane equation can be written as ax+by+cz = constant. By thinking of
	numbers as 1x1 matrices, we can write that as

		         _   _
		        |  x  |
		[a b c] |  y  | = constant.
		        |_ z _|

	Here we have

		            _   _
		       T   |  a  |
		[a b c]  = |  b  | = pl->normal.
		           |_ c _|

	How to apply a matrix to the plane? Consider the two planes that we should
	have before and after applying the matrix. A point is on the plane after
	applying the transform if and only if the INVERSE transformed point is on the
	plane before applying the transform. This means that the plane we have after
	the transform has the equation

		              _   _
		             |  x  |
		[a b c] M^-1 |  y  | = constant,
		             |_ z _|

	and from linear algebra, we know that

		                           _   _    T
		               /       T  |  a  | \
		[a b c] M^-1 = | (M^-1)   |  b  | |
		               \          |_ c _| /
	*/
	transpose(&inverse);
	pl->normal = mat3_mul_vec3(inverse, pl->normal);
}

void plane_move(struct Plane *pl, Vec3 mv)
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

float plane_point_distanceSQUARED(struct Plane pl, Vec3 pt)
{
	/*
	3D version of this: https://akuli.github.io/math-derivations/analytic-plane-geometry/distance-line-point.html

	Constant has minus sign because it's written to different side of equation
	here than in the link.
	*/
	float top = vec3_dot(pl.normal, pt) - pl.constant;
	return top*top / vec3_lengthSQUARED(pl.normal);
}


bool line_intersect_plane(struct Line ln, struct Plane pl, Vec3 *res)
{
	float dot = vec3_dot(ln.dir, pl.normal);
	//if (fabsf(dot) < 1e-5f) {
	if (dot == 0) {
		// ln.normal and pl.dir perpendicular, so line and plane are parallel
		return false;
	}

	/*
	ln equation: (x,y,z) = ln.point + number*ln.dir
	pl equation: (x,y,z) dot pl.normal = pl.constant

	Plugging equations into each other and solving gives this
	*/
	*res = ln.point;
	float number = (pl.constant - vec3_dot(ln.point, pl.normal))/dot;
	*res = vec3_add(ln.point, vec3_mul_float(ln.dir, number));
	return true;
}

float line_point_distanceSQUARED(struct Line ln, Vec3 pt)
{
	// any vector from line to pt
	Vec3 line2point = vec3_sub(pt, ln.point);

	// calculate area of parallelogram with line2point and ln.dir as sides
	float areaSQUARED = vec3_lengthSQUARED(vec3_cross(line2point, ln.dir));

	// area = base * height = |ln.dir| * distance
	return areaSQUARED / vec3_lengthSQUARED(ln.dir);
}
