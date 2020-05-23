// vectors and matrices (no matrices yet)

#ifndef VECMAT_H
#define VECMAT_H

struct Vec3 { float x,y,z; };

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


#endif   // VECMAT_H
