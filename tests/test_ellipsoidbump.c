#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include "../src/linalg.h"
#include "../src/ellipsoid.h"


static bool close3(float a, float b, float maxerror)
{
	assert(maxerror > 0);
	return (fabsf(a-b) < maxerror);
}

static bool close(float a, float b)
{
	return close3(a, b, 1e-5f);
}

void test_equation_solver(void)
{
	float ellipsoid_solve_the_equation(float A, float B, float C, float D, float E);

	// (x + 1) sqrt(x^2 + 1) - 3x = 0
	float root = ellipsoid_solve_the_equation(1, 1, 1, 1, -3);
	float actual = 0.670211622520842f;   // from sympy
	assert(close(root, actual));
}

void test_origin_centered_ellipse_distance1_points_with_given_y(void)
{
	bool ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(
		float a, float b, float pointy, float *pointx1, float *pointx2);
	float x1, x2;

	assert(ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(1, 1, sqrtf(2), &x1, &x2));
	assert(x1 + x2 == 0);
	assert(close(x2, sqrtf(2)));

	assert(ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(2, 2, 2*sqrtf(2), &x1, &x2));
	assert(x1 + x2 == 0);
	assert(close(x2, 1));

	assert(ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(1, 1, 2, &x1, &x2));
	assert(close(x1, 0));
	assert(close(x2, 0));

	assert(!ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(1,   2,  3.1f, &x1, &x2));
	assert(!ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(0.5, 2,  3.1f, &x1, &x2));
	assert(!ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(2,   2,  3.1f, &x1, &x2));
	assert(!ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(1,   2, -3.1f, &x1, &x2));
	assert(!ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(0.5, 2, -3.1f, &x1, &x2));
	assert(!ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(2,   2, -3.1f, &x1, &x2));

	assert(ellipsoid_origin_centered_ellipse_distance1_points_with_given_y(2, 3, 2.5, &x1, &x2));
	/*
	Asserted x coordinates measured by plotting and then zooming such that one
	math unit corresponds to the width of my finger.
	*/
	assert(close3(x1, -2.3f, 0.1f));
	assert(close3(x2, 2.3f, 0.1f));
}

void test_ellipsoid_2d_move_amount_x_for_origin_centered_unit_circle(void)
{
	float ellipsoid_2d_move_amount_x_for_origin_centered_unit_circle(float a, float b, Vec2 center);

	// ellipse equations and correct results come from experimenting with a grapher
	assert(ellipsoid_2d_move_amount_x_for_origin_centered_unit_circle(2, 2.5, (Vec2){2, 3}) == 0);
	assert(ellipsoid_2d_move_amount_x_for_origin_centered_unit_circle(2, 2.5, (Vec2){-2, 3}) == 0);

	float mv = ellipsoid_2d_move_amount_x_for_origin_centered_unit_circle(2, 2.5, (Vec2){1, 3});
	assert(mv > 0);
	assert(close3(mv, 0.6f, 0.1f));

	mv = ellipsoid_2d_move_amount_x_for_origin_centered_unit_circle(2, 2.5, (Vec2){-1, 3});
	assert(mv > 0);
	assert(close3(mv, 0.6f, 0.1f));
}

static float checked_2d_line_and_circle_thing(float centerx, float centery, float halflen)
{
	float ellipsoid_2d_line_and_unit_circle_move_amount(Vec2 center, float halflen);
	float res1 = ellipsoid_2d_line_and_unit_circle_move_amount((Vec2){centerx,centery}, halflen);
	float res2 = ellipsoid_2d_line_and_unit_circle_move_amount((Vec2){-centerx,centery}, halflen);
	assert(res1 == res2);
	return res1;
}

void test_ellipsoid_2d_line_and_unit_circle_move_amount(void)
{
	assert(close(checked_2d_line_and_circle_thing(1+cosf(1), sinf(1), 1), 0));
	assert(close(checked_2d_line_and_circle_thing(1+cosf(1), sinf(1), 7), 6));
	assert(close(checked_2d_line_and_circle_thing(4+cosf(1), sinf(1), 10), 6));
	assert(close(checked_2d_line_and_circle_thing(1, sqrtf(2)/2, 1), sqrtf(2)/2));
	assert(close(checked_2d_line_and_circle_thing(1, sqrtf(2)/2, 0.5f), sqrtf(2)/2 - 0.5f));

	assert(checked_2d_line_and_circle_thing(0.5, 2, 1) == 0);
	assert(checked_2d_line_and_circle_thing(2, 0.5, 1) == 0);
}

void test_ellipsoid_bump_amount_and_hidelowerhalf_with_actual_ellipsoids(void)
{
	struct Ellipsoid upper = { .center = {0,3.1f,0}, .xzradius = 10, .yradius = 8 };
	struct Ellipsoid lower = { .center = {0,0,0}, .xzradius = 20, .yradius = 3 };
	ellipsoid_update_transforms(&upper);
	ellipsoid_update_transforms(&lower);

	assert(ellipsoid_bump_amount(&upper, &lower) > 20);
	assert(ellipsoid_bump_amount(&lower, &upper) > 20);
	lower.hidelowerhalf = true;  // ignored
	assert(ellipsoid_bump_amount(&upper, &lower) > 20);
	assert(ellipsoid_bump_amount(&lower, &upper) > 20);
	upper.hidelowerhalf = true;
	assert(ellipsoid_bump_amount(&upper, &lower) == 0);
	assert(ellipsoid_bump_amount(&lower, &upper) == 0);
}
