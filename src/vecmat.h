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

// dot product
float vec3_dot(struct Vec3 v, struct Vec3 w);

/*
Returns |v|^2. Function name has SQUARED in capital letters to make sure you
notice it. It's good to avoid square roots in performance critical code.
*/
float vec3_lengthSQUARED(struct Vec3 v);

// cross product
struct Vec3 vec3_cross(struct Vec3 v, struct Vec3 w);

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

// Move plane by vector
void plane_move(struct Plane *pl, struct Vec3 mv);


#endif   // VECMAT_H
