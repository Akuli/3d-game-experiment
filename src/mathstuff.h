// geometry stuff, mostly 3d and cartesian coordinates

#ifndef MATHSTUFF_H
#define MATHSTUFF_H

#include <stdbool.h>

#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))
#define min4(a,b,c,d) min(min(a,b),min(c,d))
#define max4(a,b,c,d) max(max(a,b),max(c,d))

// matrices are structs because that way it's easier to return them and stuff
typedef struct { float x,y; } Vec2;
typedef struct { float x,y,z; } Vec3;
typedef struct { float rows[3][3]; } Mat3;

// v+w
Vec3 vec3_add(Vec3 v, Vec3 w);

// *v += w
void vec3_add_inplace(Vec3 *v, Vec3 w);

// -v
Vec3 vec3_neg(Vec3 v);

// v-w
Vec3 vec3_sub(Vec3 v, Vec3 w);

// multiply each of x,y,z by number
Vec3 vec3_mul_float(Vec3 v, float f);

// dot product
float vec3_dot(Vec3 v, Vec3 w);

// Vector projection
Vec3 vec3_project(Vec3 projectme, Vec3 onto);

/*
Returns |v|^2. Function name has SQUARED in capital letters to make sure you
notice it. It's good to avoid square roots in performance critical code.
*/
float vec3_lengthSQUARED(Vec3 v);

/*
Return a vector in same direction as v but with the given length.

Somewhat slow because calculates sqrt.
*/
Vec3 vec3_withlength(Vec3 v, float len);

// cross product
Vec3 vec3_cross(Vec3 v, Vec3 w);

void vec3_apply_matrix(Vec3 *v, Mat3 M);

// matrix times vector
Vec3 mat3_mul_vec3(Mat3 M, Vec3 v);

// matrix times matrix
Mat3 mat3_mul_mat3(Mat3 A, Mat3 B);

// multiply each entry of the matrix by a number
Mat3 mat3_mul_float(Mat3 M, float f);

// determinant
float mat3_det(Mat3 M);

// inverse matrix
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
need to, you can use atan2 for computing the angle anyway.)
*/
Mat3 mat3_rotation_xz_sincos(float sin, float cos);


// any plane in 3D, behaves nicely no matter which way plane is oriented
struct Plane {
	// equation of plane represented as:  (x,y,z) dot normal = constant
	Vec3 normal;
	float constant;
};

// Is a point on the side of the plane pointed by the normal vector?
bool plane_whichside(struct Plane pl, Vec3 pt);

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

/*
If line and plane are parallel, returns false. Otherwise returns true and sets *res
to the intersection point.
*/
bool line_intersect_plane(struct Line ln, struct Plane pl, Vec3 *res);

// distance between line and point, ^2 to avoid slow sqrt
float line_point_distanceSQUARED(struct Line ln, Vec3 pt);


#endif   // MATHSTUFF_H
