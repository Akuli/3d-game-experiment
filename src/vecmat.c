#include "vecmat.h"

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
