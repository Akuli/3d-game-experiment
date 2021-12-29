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

void test_ellipsoid_bump_amount_without_hidelowerhalf(void)
{
	float ellipse_move_amount_x(
		float a1, float b1, Vec2 center1, bool hidelowerhalf1,
		float a2, float b2, Vec2 center2);

	// ellipse equations and correct results come from experimenting with a grapher
	assert(ellipse_move_amount_x(2, 2.5, (Vec2){2, 3}, false, 1, 1, (Vec2){0,0}) == 0);
	assert(ellipse_move_amount_x(2, 2.5, (Vec2){-2, 3}, false, 1, 1, (Vec2){0,0}) == 0);

	float mv = ellipse_move_amount_x(2, 2.5, (Vec2){1, 3}, false, 1, 1, (Vec2){0,0});
	assert(mv > 0);
	assert(close3(mv, 0.6f, 0.1f));

	mv = ellipse_move_amount_x(2, 2.5, (Vec2){-1, 3}, false, 1, 1, (Vec2){0,0});
	assert(mv > 0);
	assert(close3(mv, 0.6f, 0.1f));
}

void test_ellipsoid_bump_amount_with_hidelowerhalf(void)
{
	float ellipse_move_amount_x(
		float a1, float b1, Vec2 center1, bool hidelowerhalf1,
		float a2, float b2, Vec2 center2);

	assert(close(ellipse_move_amount_x(1, 1, (Vec2){1+cosf(1), sinf(1)}, true, 1, 1, (Vec2){0,0}), 0));
	assert(close(ellipse_move_amount_x(7, 1, (Vec2){1+cosf(1), sinf(1)}, true, 1, 1, (Vec2){0,0}), 6));
	assert(close(ellipse_move_amount_x(10, 1, (Vec2){4+cosf(1), sinf(1)}, true, 1, 1, (Vec2){0,0}), 6));
	assert(close(ellipse_move_amount_x(1, 1, (Vec2){1, sqrtf(2)/2}, true, 1, 1, (Vec2){0,0}), sqrtf(2)/2));
	assert(close(ellipse_move_amount_x(0.5f, 1, (Vec2){1, sqrtf(2)/2}, true, 1, 1, (Vec2){0,0}), sqrtf(2)/2 - 0.5f));
	assert(close(ellipse_move_amount_x(1, 1, (Vec2){0.5f,2}, true, 1, 1, (Vec2){0,0}), 0));
	assert(close(ellipse_move_amount_x(1, 1, (Vec2){2,0.5f}, true, 1, 1, (Vec2){0,0}), 0));
}

void test_ellipsoid_bump_amount_hidelowerhalf_with_actual_ellipsoids(void)
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
