#ifndef MATHSTUFF_H
#define MATHSTUFF_H

#include <math.h>
#include <stdbool.h>

// windows.h defines min and max macros just like the ones below
#if !defined(min) && !defined(max)
	#define min(a,b) ((a)<(b) ? (a) : (b))
	#define max(a,b) ((a)>(b) ? (a) : (b))
#endif

inline void clamp(int *val, int lo, int hi) {
	if (*val < lo) *val = lo;
	if (*val > hi) *val = hi;
}
inline void clamp_float(float *val, float lo, float hi) {
	if (*val < lo) *val = lo;
	if (*val > hi) *val = hi;
}

// matrices are structs because that way it's easier to return them and stuff
typedef struct { float x,y; } Vec2;
typedef struct { float x,y,z; } Vec3;
typedef struct { float rows[3][3]; } Mat3;

// Inlined stuff typically goes to tight loops

inline Vec3 vec3_add(Vec3 v, Vec3 w) { return (Vec3){ v.x+w.x, v.y+w.y, v.z+w.z }; }
inline Vec3 vec3_sub(Vec3 v, Vec3 w) { return (Vec3){ v.x-w.x, v.y-w.y, v.z-w.z }; }
inline void vec3_add_inplace(Vec3 *v, Vec3 w) { v->x += w.x; v->y += w.y; v->z += w.z; }
inline void vec3_sub_inplace(Vec3 *v, Vec3 w) { v->x -= w.x; v->y -= w.y; v->z -= w.z; }
inline Vec3 vec3_mul_float(Vec3 v, float f) { return (Vec3){ v.x*f, v.y*f, v.z*f }; }
inline float vec3_dot(Vec3 v, Vec3 w) { return v.x*w.x + v.y*w.y + v.z*w.z; }

/*
Returns |v|^2. Function name has SQUARED in capital letters to make sure you
notice it. It's good to avoid square roots in performance critical code.
*/
inline float vec3_lengthSQUARED(Vec3 v) { return vec3_dot(v, v); }

/*
Return a vector in same direction as v but with the given length.

Direction gets reversed if len is negative. Somewhat slow because calculates sqrt.
*/
inline Vec3 vec3_withlength(Vec3 v, float len)
{
	return vec3_mul_float(v, len/sqrtf(vec3_lengthSQUARED(v)));
}

// cross product
inline Vec3 vec3_cross(Vec3 v, Vec3 w)
{
	/*
	| i j k |    | b c |    | a c |    | a b |
	| a b c | = i| e f | - j| d f | + k| d e |
	| d e f |
	          = (bf-ce)i - (af-cd)j + (ae-bd)k
	*/
	float a = v.x, b = v.y, c = v.z;
	float d = w.x, e = w.y, f = w.z;
	return (Vec3) {
		.x = b*f - c*e,
		.y = -(a*f - c*d),
		.z = a*e - b*d,
	};
}

// matrix * vector
inline Vec3 mat3_mul_vec3(Mat3 M, Vec3 v) { return (Vec3){
	v.x*M.rows[0][0] + v.y*M.rows[0][1] + v.z*M.rows[0][2],
	v.x*M.rows[1][0] + v.y*M.rows[1][1] + v.z*M.rows[1][2],
	v.x*M.rows[2][0] + v.y*M.rows[2][1] + v.z*M.rows[2][2],
};}

inline void vec3_apply_matrix(Vec3 *v, Mat3 M) { *v = mat3_mul_vec3(M, *v); }

// these are not inline because typically you don't put these to tight loop:
Mat3 mat3_mul_mat3(Mat3 A, Mat3 B);
Mat3 mat3_inverse(Mat3 M);

/*
Slow-ish to compute because uses trig funcs, so don't call this in a loop.

The angle is in radians. If x axis is right, y is up, and z is back, then this
works so that a bigger angle means clockwise if viewed from above. If viewed from
below, then it rotates counter-clockwise instead, so it's just like unit circle
trig in high school, except that you have z axis instead of y axis. For example:
- angle=-pi/2 means that (1,0,0) gets rotated to (0,0,-1)
- angle=0 gives identity matrix
- angle=pi/2 means that (1,0,0) gets rotated to (0,0,1)
- angle=pi means that (1,0,0) gets rotated to (-1,0,0)
*/
Mat3 mat3_rotation_xz(float angle);

/*
Like mat3_rotation_xz, but takes cos(angle) and sin(angle) instead of angle. Useful
if you have cos and sin but you don't want to compute the angle. (If you really
need to, you can use atan2f for computing the angle anyway.)
*/
Mat3 mat3_rotation_xz_sincos(float sin, float cos);


// any plane in 3D, behaves nicely no matter which way plane is oriented
struct Plane {
	// equation of plane represented as:  (x,y,z) dot normal = constant
	Vec3 normal;
	float constant;
};

// Is a point on the side of the plane pointed by the normal vector?
inline bool plane_whichside(struct Plane pl, Vec3 pt) { return (vec3_dot(pl.normal, pt) > pl.constant); }

// Apply the inverse of the given matrix to every point of the plane
void plane_apply_mat3_INVERSE(struct Plane *pl, Mat3 inverse);

// Move plane by vector
void plane_move(struct Plane *pl, Vec3 mv);

// distance between plane and point, ^2 to avoid slow sqrt
float plane_point_distanceSQUARED(struct Plane pl, Vec3 pt);


struct Line {
	Vec3 point;       // any point on the line
	Vec3 dir;         // nonzero vector going in direction of the line
};

// distance between line and point, ^2 to avoid slow sqrt
float line_point_distanceSQUARED(struct Line ln, Vec3 pt);


#endif   // MATHSTUFF_H
