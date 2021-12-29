// Many functions in this file are not static because tests.

#include "ellipsoid.h"
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "log.h"
#include "linalg.h"
#include "misc.h"

/*
Finds the maximum value of

	f(x) = sqrt(A1 x^2 + B1 x + C1) + sqrt(A2 x^2 + B2 x + C2)

on an interval [a,b].
*/
static float find_max_value(float A1, float B1, float C1, float A2, float B2, float C2, float a, float b)
{
	SDL_assert(a <= b);

	// https://en.wikipedia.org/wiki/Golden-section_search
	// a < t < u < b (if a=b, loop below does nothing)
	float phi = 1.618033988749895f;
	float t = a + (2-phi)*(b-a);
	float u = a + (-1+phi)*(b-a);

#define f(x) (sqrtf(A1*x*x + B1*x + C1) + sqrtf(A2*x*x + B2*x + C2))
	float ft = f(t);
	float fu = f(u);

	int iter = 0;  // just in case of nan weirdness, lol
	while (b-a > 1e-3f) {  // Function values more precise because derivative zero at max
		// It usually does about 20 iterations, depending only on length of input interval
		if (iter++ == 25) {
			log_printf(
				"hitting max number of iterations: "
				"f(x) = sqrt(%fx^2 + %fx + %f) + sqrt(%fx^2 + %fx + %f) on [%f,%f]",
				A1,B1,C1,A2,B2,C2,a,b);
			break;
		}
		if (ft < fu) {
			// Increases between t and u, max can't be on [a,t]
			a = t;
			t = u;  // magic constants using phi chosen so that this works
			ft = fu;
			u = a + (-1+phi)*(b-a);
			fu = f(u);
		} else {
			b = u;
			u = t;
			fu = ft;
			t = a + (2-phi)*(b-a);
			ft = f(t);
		}
	}
	return (ft+fu)/2;
#undef f
}

/*
Calculates how much, along a horizontal line, the ellipsoids

	((x - center1.x)/a1)^2 + ((y - center1.y)/b1)^2 = 1
	((x - center2.x)/a2)^2 + ((y - center2.y)/b2)^2 = 1

intersect, where the line is chosen to make them intersect as little as
possible. Negative result means no intersection.
*/
static float max_x_intersection_between_ellipsoids(float a1, float b1, Vec2 center1, float a2, float b2, Vec2 center2)
{
	// [a,b] = overlapping y coordinates of the two ellipses
	float a = max(center1.y - b1, center2.y - b2);
	float b = min(center1.y + b1, center2.y + b2);
	if (a > b)
		return -1;  // no overlap in y direction

	/*
	If center1.x < center2.x, we use right edge of ellipsoid 1 and left
	edge of ellipsoid 2. Their distance along a horizontal line at height y:

		  (center1.x + sqrt(a1^2 - (a1/b1)^2 (y - center1.y)^2))
		- (center2.x - sqrt(a2^2 - (a2/b2)^2 (y - center2.y)^2))

	If center1.x > center2.x, we get a similar thing with a couple signs
	flipped.
	*/
	float k1 = (a1*a1)/(b1*b1);
	float A1 = -k1;
	float B1 = 2*k1*center1.y;
	float C1 = a1*a1 - k1*center1.y*center1.y;

	float k2 = (a2*a2)/(b2*b2);
	float A2 = -k2;
	float B2 = 2*k2*center2.y;
	float C2 = a2*a2 - k2*center2.y*center2.y;

	return find_max_value(A1, B1, C1, A2, B2, C2, a, b) - fabsf(center1.x - center2.x);
}

// Special case of the previous function: b1 = 0
// Needs to be special cased because division by zero and stuff.
static float max_x_intersection_between_line_and_ellipsoid(float a1, Vec2 center1, float a2, float b2, Vec2 center2)
{
	float ydiffrelative = (center1.y - center2.y)/b2;  // y coords overlap <=> this is between -1 and +1
	float tmp = 1 - ydiffrelative*ydiffrelative;
	if (tmp < 0)
		return -1;
	return -fabsf(center1.x - center2.x) + a1 + sqrtf(a2*a2*tmp);
}

float ellipse_move_amount_x(
	float a1, float b1, Vec2 center1, bool hidelowerhalf1,
	float a2, float b2, Vec2 center2)
{
	SDL_assert(a1 > 0);
	SDL_assert(b1 > 0);
	SDL_assert(a2 > 0);
	SDL_assert(b2 > 0);

	/*
		el1

		el2
	*/
	SDL_assert(center1.y >= center2.y || fabsf(center1.y - center2.y) < 1e-5f);

	float xdiff;
	if (hidelowerhalf1)
		xdiff = max_x_intersection_between_line_and_ellipsoid(a1, center1, a2, b2, center2);
	else
		xdiff = max_x_intersection_between_ellipsoids(a1, b1, center1, a2, b2, center2);
	return max(xdiff, 0);
}

float ellipsoid_bump_amount(const struct Ellipsoid *el1, const struct Ellipsoid *el2)
{
	// optimization for common case
	float d = max(el1->xzradius, el1->yradius) + max(el2->xzradius, el2->yradius);
	if (vec3_lengthSQUARED(vec3_sub(el1->center, el2->center)) > d*d)
		return 0;

	if (el1->center.y < el2->center.y) {
		const struct Ellipsoid *tmp = el1;
		el1 = el2;
		el2 = tmp;
	}

	Vec3 diff = vec3_sub(el1->center, el2->center);
	float cos = diff.x/hypotf(diff.x, diff.z);
	float sin = diff.z/hypotf(diff.x, diff.z);
	if (!isfinite(cos) || !isfinite(sin)) {
		// Ellipsoids lined up vertically, just move in some direction lol
		cos = 1;
		sin = 0;
	}

	// Rotate centers so that ellipsoid centers have same z coordinate
	Mat3 rot = mat3_inverse(mat3_rotation_xz_sincos(sin, cos));
	Vec3 center1 = mat3_mul_vec3(rot, el1->center);
	Vec3 center2 = mat3_mul_vec3(rot, el2->center);
	SDL_assert(fabsf(center1.z - center2.z) < 1e-5f);

	// Now this is a 2D problem on the xy plane (or some other plane parallel to xy plane)
	Vec2 center1_xy = { center1.x, center1.y };
	Vec2 center2_xy = { center2.x, center2.y };
	return ellipse_move_amount_x(
		el1->xzradius, el1->yradius, center1_xy, el1->hidelowerhalf,
		el2->xzradius, el2->yradius, center2_xy);
}
