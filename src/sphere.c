#include "sphere.h"
#include <stdbool.h>
#include "display.h"
#include "vecmat.h"

/*
Note that this returns distance^2, not distance. This avoids having to calculate a
square root.
*/
static float distance_between_point_and_line_SQUARED(
	const struct Vec3 point, const struct DisplayLine ln)
{
	// pick any two different points on the line
	struct Vec3 linep1 = displayline_z2point(ln, 0);
	struct Vec3 linep2 = displayline_z2point(ln, 1);

	// create two different vectors from the given point to the line
	struct Vec3 diff1 = vec3_sub(linep1, point);
	struct Vec3 diff2 = vec3_sub(linep2, point);

	// calculate area^2 of a parallelogram with these vectors as sides
	float areaSQUARED = vec3_lengthSQUARED(vec3_cross(diff1, diff2));

	/*
	This area is same as area of rectangle with points on lines as two corners and
	distance to line as one of the sides.
	*/
	return areaSQUARED / vec3_lengthSQUARED(vec3_sub(linep1, linep2));
}

bool sphere_touches_displayline(struct Sphere sph, struct DisplayLine ln)
{
	float distSQUARED = distance_between_point_and_line_SQUARED(sph.center, ln);
	return (distSQUARED <= sph.radius*sph.radius);
}
