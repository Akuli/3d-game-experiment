// vectors and everything related
// TODO: rename this file

#ifndef VECMAT_H
#define VECMAT_H

#include <stdbool.h>

struct Vec3 { float x,y,z; };

// v+w
struct Vec3 vec3_add(struct Vec3 v, struct Vec3 w);

// -v
struct Vec3 vec3_neg(struct Vec3 v);

// v-w
struct Vec3 vec3_sub(struct Vec3 v, struct Vec3 w);

// multiply each of x,y,z by number
struct Vec3 vec3_mul_float(struct Vec3 v, float f);

// dot product
float vec3_dot(struct Vec3 v, struct Vec3 w);

/*
Returns |v|^2. Function name has SQUARED in capital letters to make sure you
notice it. It's good to avoid square roots in performance critical code.
*/
float vec3_lengthSQUARED(struct Vec3 v);

/*
Return a vector in same direction as v but with the given length.

Somewhat slow because calculates sqrt.
*/
struct Vec3 vec3_withlength(struct Vec3 v, float len);

// cross product
struct Vec3 vec3_cross(struct Vec3 v, struct Vec3 w);


// wrapped in a struct to make it possible to return or assign a matrix
struct Mat3 {
	float rows[3][3];
};

// matrix times vector
struct Vec3 mat3_mul_vec3(struct Mat3 M, struct Vec3 v);

// matrix times matrix
struct Mat3 mat3_mul_mat3(struct Mat3 A, struct Mat3 B);

// multiply each entry of the matrix by a number
struct Mat3 mat3_mul_float(struct Mat3 M, float f);

// determinant
float mat3_det(struct Mat3 M);

// inverse matrix
struct Mat3 mat3_inverse(struct Mat3 M);

// slow to compute because uses trig funcs, so don't call this in a loop
// angle is in radians, like everything
struct Mat3 mat3_rotation_xz(float angle);


// any plane in 3D, behaves nicely no matter which way plane is oriented
struct Plane {
	// equation of plane represented as:  (x,y,z) dot normal = constant
	struct Vec3 normal;
	float constant;
};

/*
Is a point on the side of the plane pointed by the normal vector?
*/
bool plane_whichside(struct Plane pl, struct Vec3 pt);

// Apply the inverse of the given matrix to every point of the plane
void plane_apply_mat3_INVERSE(struct Plane *pl, struct Mat3 inverse);

// Move plane by vector
void plane_move(struct Plane *pl, struct Vec3 mv);

// distance between plane and point, never negative
// Somewhat slow because calculates sqrt.
float plane_point_distance(struct Plane pl, struct Vec3 pt);


#endif   // VECMAT_H
