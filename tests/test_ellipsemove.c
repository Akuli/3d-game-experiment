#include <assert.h>
#include "../src/ellipsemove.c"


static bool close3(float a, float b, float maxerror)
{
	assert(maxerror > 0);
	return (fabsf(a-b) < maxerror);
}

static bool close(float a, float b)
{
	// PRECISION_REQUIREMENT comes from ../src/ellipsemove.c
	return close3(a, b, PRECISION_REQUIREMENT);
}

static void test_equation_solver(void)
{
	// (x + 1) sqrt(x^2 + 1) - 3x = 0
	float root = solve_the_equation(1, 1, 1, 1, -3);
	float actual = 0.670211622520842f;   // from sympy
	assert(close(root, actual));
}

static void test_origin_centered_ellipse_distance1_points_with_given_y(void)
{
	float x1, x2;

	assert(origin_centered_ellipse_distance1_points_with_given_y(1, 1, sqrtf(2), &x1, &x2));
	assert(x1 + x2 == 0);
	assert(close(x2, sqrtf(2)));

	assert(origin_centered_ellipse_distance1_points_with_given_y(2, 2, 2*sqrtf(2), &x1, &x2));
	assert(x1 + x2 == 0);
	assert(close(x2, 1));

	assert(origin_centered_ellipse_distance1_points_with_given_y(1, 1, 2, &x1, &x2));
	assert(close(x1, 0));
	assert(close(x2, 0));

	assert(!origin_centered_ellipse_distance1_points_with_given_y(1,   2,  3.1f, &x1, &x2));
	assert(!origin_centered_ellipse_distance1_points_with_given_y(0.5, 2,  3.1f, &x1, &x2));
	assert(!origin_centered_ellipse_distance1_points_with_given_y(2,   2,  3.1f, &x1, &x2));
	assert(!origin_centered_ellipse_distance1_points_with_given_y(1,   2, -3.1f, &x1, &x2));
	assert(!origin_centered_ellipse_distance1_points_with_given_y(0.5, 2, -3.1f, &x1, &x2));
	assert(!origin_centered_ellipse_distance1_points_with_given_y(2,   2, -3.1f, &x1, &x2));

	assert(origin_centered_ellipse_distance1_points_with_given_y(2, 3, 2.5, &x1, &x2));
	/*
	Asserted x coordinates measured by plotting and then zooming such that one
	math unit corresponds to the width of my finger.
	*/
	assert(close3(x1, -2.3f, 0.1f));
	assert(close3(x2, 2.3f, 0.1f));
}

static void test_ellipse_move_amount_x_for_origin_centered_unit_circle(void)
{
	// ellipse equations and correct results come from experimenting with a grapher
	assert(ellipse_move_amount_x_for_origin_centered_unit_circle(2, 2.5, (Vec2){2, 3}) == 0);
	assert(ellipse_move_amount_x_for_origin_centered_unit_circle(2, 2.5, (Vec2){-2, 3}) == 0);

	float mv = ellipse_move_amount_x_for_origin_centered_unit_circle(2, 2.5, (Vec2){1, 3});
	assert(mv > 0);
	assert(close3(mv, 0.6f, 0.1f));

	mv = ellipse_move_amount_x_for_origin_centered_unit_circle(2, 2.5, (Vec2){-1, 3});
	assert(mv > 0);
	assert(close3(mv, 0.6f, 0.1f));
}

int main(void)
{
	test_equation_solver();
	test_origin_centered_ellipse_distance1_points_with_given_y();
	test_ellipse_move_amount_x_for_origin_centered_unit_circle();
	printf("ok\n");
	return 0;
}
