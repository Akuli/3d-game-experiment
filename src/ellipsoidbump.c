// Many functions in this file are not static because tests.

#include "ellipsoid.h"
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "log.h"
#include "linalg.h"

/*
With 1e-7f, tests sometimes hit the max number of iterations. This didn't happen
when I tried 1e-6f. I'm setting this to even less demanding to be on the safe side.
*/
#define PRECISION_REQUIREMENT 1e-5f

/*
Solves the equation f(x)=0 for -1 <= x <= 1, where

	f(x) = (Ax + B) sqrt(Cx^2 + D) + Ex.
*/
float ellipsoid_solve_the_equation(float A, float B, float C, float D, float E)
{
	/*
	This initial guessing thing is very important. I didn't have much luck without:
	- If I squared both sides to make it a 4th degree polynomial, then squaring
	  both sides gave me "fake solutions". This is why your math teacher wants you
	  to be careful when you ^2 both sides.
	- Without squaring both sides, I got sqrt(x^2 - bla) type thing which
	  wasn't defined for all x. Sometimes Newton's method wanted to suggest a value
	  outside the interval where it was defined.
	*/

	float xguess = 0;   // should be never actually used, makes compiler happy
	float absmin = HUGE_VALF;

	float guessingstep = 0.1f;
	for (float x = -1; x <= 1; x += guessingstep) {
		float f = (A*x + B)*sqrtf(C*x*x + D) + E*x;
		if(!isfinite(f)) {
			log_printf("got f(x) = %.10f. Arguments: A=%.10f B=%.10f C=%.10f D=%.10f E=%.10f",
				f, A, B, C, D, E);
			continue;
		}

		float absval = fabsf(f);
		if (absval < absmin) {
			absmin = absval;
			xguess = x;
		}
	}

	/*
	Now newton's method to refine our guess quickly. Works well most of the time,
	but some special cases occur if you jump a lot with both players by smashing
	buttons on keyboard.
	*/
	float x = xguess;
	int iter = 0;
	float sub;
	do {

#define LOG(MSG) \
	log_printf("%s (iter=%d x=%.10f xguess=%.10f A=%.10f B=%.10f C=%.10f D=%.10f E=%.10f)", \
	(MSG), iter, x, xguess, A, B, C, D, E)

		/*
		Could use a squared-both-sides version of the equation here to avoid
		calculating square roots, but this works quickly enough
		*/
		float sqrtstuff = sqrtf(C*x*x + D);
		float sqrtstuff_derivative = C*x/sqrtstuff;
		float f = (A*x + B)*sqrtstuff + E*x;
		float f_derivative = A*sqrtstuff + (A*x + B)*sqrtstuff_derivative + E;
		if (f_derivative == 0) {
			LOG("derivative is zero");
			return xguess;
		}

		// newton's method formula from high school: x_(n+1) = x_n - f(x_n)/f'(x_n)
		// this doesn't behave nicely if derivative is zero or small
		sub = f / f_derivative;
		if (!isfinite(sub)) {
			LOG("division by nonzero derivative gave something weird");
			return xguess;
		}
		x -= sub;

		if (fabs(x - xguess) > 2*guessingstep) {
			LOG("x value is far away from guess");
			return xguess;
		}
		if (fabsf(x) > 1) {
			if (fabsf(x) > 1.01)
				LOG("x value not between -1 and 1");
			return xguess;
		}

		// it typically takes 3 or 4 iterations to make it precise, so this limit is plenty
		if (++iter == 20) {
			LOG("hitting max number of iterations");
			break;
		}
#undef LOG
	} while (fabsf(sub) > PRECISION_REQUIREMENT);

	return x;
}

/*
Find the x coordinates of the points that are distance 1 away from the
origin-centered ellipse

	(x/a)^2 + (y/b)^2 = 1

given the y coordinate of the points. This returns false if there are no points.
When this returns true, you always get *pointx1 <= 0 and *pointx2 >= 0.
*/
bool ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(
	float a, float b, float pointy, float *pointx1, float *pointx2)
{
	SDL_assert(a > 0);
	SDL_assert(b > 0);

	// Ensure that solution exists: b+1 is how far up or down from x axis the points can be
	if (fabsf(pointy) > b+1)
		return false;

	/*
	We parametrize the ellipse as (x,y) = E(t), where

		E(t) = (a cos(t), b sin(t)).

	As t goes from 0 to 2pi, this point goes around the ellipse counter-clockwise.
	With a=b=1, this is the high-school unit circle, and here a and b are just
	stretching that.

	We measure the distance from ellipse to (pointx,pointy) perpendicularly to the
	ellipse. A vector going counter-clockwise along the ellipse is given by

		E'(t) = (-a sin(t), b cos(t)).

	Consider the rotation

		rotate90clockwise(x, y) = (y, -x).

	The vector

		A(t) = rotate90clockwise(E'(t)) = (b cos(t), a sin(t))

	is pointing away from the ellipse perpendicularly. Now we want

		E(t) + A(t)/|A(t)| = (pointx, pointy)

	where A(t)/|A(t)| is a vector of length 1 pointing away from the ellipse.
	Comparing y coordinates of the vectors and simplifying gives

		(A sin(t) + B) sqrt(C sin^2(t) + D) + E sin(t) = 0

	with the following constants:
	*/
	float A = -b;
	float B = pointy;
	float C = a*a - b*b;
	float D = b*b;
	float E = -a;
	float sint = ellipsoid_solve_the_equation(A, B, C, D, E);

	/*
	Above we had an equation of vectors, and so far we used only the y coordinates
	part of it. The x coordinate part gives

		pointx = ( a + b/sqrt(a^2 sin^2(t) + b^2 cos^2(t)) ) cos(t).

	We have cos(t) = +- sqrt(1 - sin^2(t)), with two choices, and hence we get two
	different x coordinates symmetrically as expected.
	*/
	float costSQUARED = 1 - sint*sint;
	*pointx2 = ( a + b/sqrtf(a*a*sint*sint + b*b*costSQUARED) )*sqrtf(costSQUARED);
	*pointx1 = -*pointx2;
	return true;
}

/*
How much should the ellipse move in x direction to make it not intersect the
origin-centered unit circle?
*/
float ellipsoid_2d_move_amount_x_for_origin_centered_unit_circle(
	float a, float b, Vec2 center)
{
	// If 0 is between these, then the unit circle and ellipse intersect
	float xmin, xmax;

	/*
	For calling this function, we need to shift coordinates so that ellipse is at
	origin. The unit circle's location is (-center.x, -center.y) in those
	coordinates.
	*/
	if (!ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(
			a, b, -center.y, &xmin, &xmax))
	{
		return 0;
	}
	SDL_assert(xmin <= xmax);

	// Convert back to original coordinates
	xmin += center.x;
	xmax += center.x;
	SDL_assert(xmin <= xmax);

	if (!( xmin < 0 && 0 < xmax ))
		return 0;

	// Ellipse should move
	if (center.x > 0) {
		// Ellipse should move right
		SDL_assert(xmin < 0);
		return fabsf(xmin);
	} else {
		// Ellipse should move left
		SDL_assert(xmax > 0);
		return xmax;
	}
}

/*
How much to move unit circle and a horizontal line apart to make them not intersect?
Line goes from (center.x, center.y - halflen) to (center.x, center.y + halflen).
*/
float ellipsoid_2d_line_and_unit_circle_move_amount(Vec2 linecenter, float halflen)
{
	float tmp = 1 - linecenter.y*linecenter.y;
	if (tmp < 0)  // line above/below unit circle
		return 0;

	float res = sqrtf(tmp) - fabsf(linecenter.x) + halflen;
	if (res < 0)  // line too far left/right from unit circle
		return 0;
	return res;
}

static float ellipse_move_amount_x_without_hidelowerhalf(
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

	// Shift and stretch coordinates so that ellipse 2 is origin-centered unit circle
	float a1new = a1 / a2;
	float b1new = b1 / b2;
	Vec2 center1new = { (center1.x - center2.x)/a2, (center1.y - center2.y)/b2 };

	float xdiff;
	if (hidelowerhalf1)
		xdiff = ellipsoid_2d_line_and_unit_circle_move_amount(center1new, a1new);
	else
		xdiff = ellipsoid_2d_move_amount_x_for_origin_centered_unit_circle(a1new, b1new, center1new);

	// Result is difference of x coords, unaffected by shifting, but must be unstretched
	return xdiff * a2;
}

float ellipsoid_bump_amount(const struct Ellipsoid *el1, const struct Ellipsoid *el2)
{
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
	return ellipse_move_amount_x_without_hidelowerhalf(
		el1->xzradius, el1->yradius, center1_xy, el1->epic->hidelowerhalf,
		el2->xzradius, el2->yradius, center2_xy);
}
