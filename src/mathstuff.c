#include "mathstuff.h"
#include <math.h>
#include <SDL2/SDL.h>

/*
Non-static inline functions are weird in c. You need to put definition to h file
and declaration to c file.
*/
extern inline void clamp(int *val, int lo, int hi);
extern inline void clamp_float(float *val, float lo, float hi);
extern inline Vec3 vec3_add(Vec3 v, Vec3 w);
extern inline Vec3 vec3_sub(Vec3 v, Vec3 w);
extern inline void vec3_add_inplace(Vec3 *v, Vec3 w);
extern inline void vec3_sub_inplace(Vec3 *v, Vec3 w);
extern inline Vec3 vec3_mul_float(Vec3 v, float f);
extern inline float vec3_dot(Vec3 v, Vec3 w);
extern inline float vec3_lengthSQUARED(Vec3 v);
extern inline Vec3 vec3_withlength(Vec3 v, float len);
extern inline Vec3 vec3_cross(Vec3 v, Vec3 w);
extern inline Vec3 mat3_mul_vec3(Mat3 M, Vec3 v);
extern inline void vec3_apply_matrix(Vec3 *v, Mat3 M);
extern inline bool plane_whichside(struct Plane pl, Vec3 pt);

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


float line_point_distanceSQUARED(struct Line ln, Vec3 pt)
{
	// any vector from line to pt
	Vec3 line2point = vec3_sub(pt, ln.point);

	// calculate area of parallelogram with line2point and ln.dir as sides
	float areaSQUARED = vec3_lengthSQUARED(vec3_cross(line2point, ln.dir));

	// area = base * height = |ln.dir| * distance
	return areaSQUARED / vec3_lengthSQUARED(ln.dir);
}

static inline Vec2 vec2_add(Vec2 a, Vec2 b) { return (Vec2){ a.x+b.x, a.y+b.y }; }
static inline Vec2 vec2_sub(Vec2 a, Vec2 b) { return (Vec2){ a.x-b.x, a.y-b.y }; }
static inline Vec2 vec2_mul_float(Vec2 a, float f) { return (Vec2){ a.x*f, a.y*f }; }
static inline float vec2_dot(Vec2 a, Vec2 b) { return a.x*b.x + a.y*b.y; }

typedef struct { float rows[2][2]; } Mat2;

static inline Vec2 mat2_mul_vec2(Mat2 M, Vec2 v) { return (Vec2){
	v.x*M.rows[0][0] + v.y*M.rows[0][1],
	v.x*M.rows[1][0] + v.y*M.rows[1][1],
};}

static inline Mat2 mat2_inverse(Mat2 M) {
	float det = M.rows[0][0]*M.rows[1][1] - M.rows[0][1]*M.rows[1][0];
	return (Mat2){ .rows = {
		{ M.rows[1][1] / det, -M.rows[0][1] / det },
		{ -M.rows[1][0] / det, M.rows[0][0] / det },
	}};
}

bool intersect_line_segments(Vec2 start1, Vec2 end1, Vec2 start2, Vec2 end2, Vec2 *res)
{
	Vec2 dir1 = vec2_sub(end1, start1);
	Vec2 dir2 = vec2_sub(end2, start2);
	if (vec2_dot(dir1, dir2) < 0) {
		dir2.x *= -1;
		dir2.y *= -1;
		Vec2 tmp = start2;
		start2 = end2;
		end2 = tmp;
	}

	float dirdet = dir1.x*dir2.y - dir2.x*dir1.y;
	if (fabsf(dirdet) < 1e-5) {
		// same direction
		Vec2 perpdir = { dir1.y, -dir1.x };
		if (fabsf(vec2_dot(perpdir, start1) - vec2_dot(perpdir, start2)) > 1e-5f)
			return false;  // far apart

		// proj(v) = (projection of v onto dir1)*length(dir1). Length doesn't affect anything.
		#define proj(v) vec2_dot(dir1, (v))  
		Vec2 olapstart = (proj(start1) < proj(start2)) ? start2 : start1;
		Vec2 olapend = (proj(end1) < proj(end2)) ? end1 : end2;
		if (proj(olapstart) >= proj(olapend))
			return false;
		#undef proj
		*res = vec2_mul_float(vec2_add(olapstart, olapend), 0.5f);
		return true;
	}

	/*
	At intersection start1 + t*dir1 = start2 + u*dir2, with t,u in [0,1].
	Solving t and u gives a linear system of two equations:
		 _               _   _ _
		| dir1.x  -dir2.x | | t |
		|                 | |   | = start2 - start1
		|_dir1.y  -dir2.y_| |_u_|
	*/
	Mat2 M = { .rows = {
		{ dir1.x, -dir2.x },
		{ dir1.y, -dir2.y },
	}};
	Vec2 tu = mat2_mul_vec2(mat2_inverse(M), vec2_sub(start2,start1));
	float t = tu.x;
	float u = tu.y;
	if (!(0 <= t && t <= 1 && 0 <= u && u <= 1))
		return false;
	*res = vec2_add(start1, vec2_mul_float(dir1, t));
	return true;
}

bool triangle_contains_point(Vec2 A, Vec2 B, Vec2 C, Vec2 point)
{
	/*
	A triangle is the convex combination of its points:

		point = tA + uB + (1-t-u)C,  where t,u in [0,1], t+u <= 1

	Solving t and u gives a linear equation:
		 _                _   _ _
		| A.x-C.x  B.x-C.x | | t |
		|                  | |   |  = P-C
		|_A.y-C.y  B.x-C.y_| |_u_|
	*/
	
}
