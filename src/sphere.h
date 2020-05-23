#ifndef SPHERE_H
#define SPHERE_H

#include <stdbool.h>
#include "display.h"
#include "vecmat.h"

struct Sphere {
	struct Vec3 center;
	float radius;
};

bool sphere_touches_displayline(struct Sphere sph, struct DisplayLine ln);


#endif  // SPHERE_H
