#include <assert.h>
#include <stdio.h>
#include "../src/intersections.h"


static bool checked_intersect_line_segments(Vec2 start1, Vec2 end1, Vec2 start2, Vec2 end2, Vec2 *p)
{
	Vec2 resultvecs[8] = {0};
	bool resultrets[8] = {
		intersect_line_segments(start1, end1, start2, end2, &resultvecs[0]),
		intersect_line_segments(end1, start1, start2, end2, &resultvecs[1]),
		intersect_line_segments(start1, end1, end2, start2, &resultvecs[2]),
		intersect_line_segments(end1, start1, end2, start2, &resultvecs[3]),
		intersect_line_segments(start2, end2, start1, end1, &resultvecs[4]),
		intersect_line_segments(end2, start2, start1, end1, &resultvecs[5]),
		intersect_line_segments(start2, end2, end1, start1, &resultvecs[6]),
		intersect_line_segments(end2, start2, end1, start1, &resultvecs[7]),
	};

	for (int i=0; i<8; i++) {
		assert(resultrets[i] == resultrets[0]);
		assert(resultvecs[i].x == resultvecs[0].x);
		assert(resultvecs[i].y == resultvecs[0].y);
	}

	*p = resultvecs[0];
	return resultrets[0];
}

void test_intersecting_lines(void)
{
	Vec2 p;

	// same direction
	assert(!checked_intersect_line_segments((Vec2){0,0}, (Vec2){1,1}, (Vec2){2,3}, (Vec2){3,4}, &p));
	assert(!checked_intersect_line_segments((Vec2){0,0}, (Vec2){1,1}, (Vec2){-1,0}, (Vec2){1,2}, &p));
	assert(!checked_intersect_line_segments((Vec2){0,0}, (Vec2){1,1}, (Vec2){2,2}, (Vec2){3,3}, &p));
	assert(!checked_intersect_line_segments((Vec2){0,0}, (Vec2){1,1}, (Vec2){2,2}, (Vec2){3,3}, &p));
	assert(checked_intersect_line_segments((Vec2){0,0}, (Vec2){2,2}, (Vec2){1,1}, (Vec2){3,3}, &p));
	assert(p.x == 1.5f && p.y == 1.5f);

	// different direction
	assert(!checked_intersect_line_segments((Vec2){0,0}, (Vec2){1,2}, (Vec2){1,1}, (Vec2){2,1}, &p));
	assert(checked_intersect_line_segments((Vec2){0,0}, (Vec2){1,2}, (Vec2){1,1}, (Vec2){0,1}, &p));
	assert(p.x == 0.5f && p.y == 1.0f);
}

void test_ngon_contains_point(void)
{
	bool ngon_contains_point(const Vec2 *corners, int n, const Vec2 point);

	Vec2 A = {5,1}, B = {1,3}, C = {4,5};
	for (int x = -20; x <= 20; x++) {
		if (x == 3 || x == 4) {
			assert(ngon_contains_point((Vec2[]){A, B, C}, 3, (Vec2){x,4}));
			assert(ngon_contains_point((Vec2[]){A, C, B}, 3, (Vec2){x,4}));
			assert(ngon_contains_point((Vec2[]){B, A, C}, 3, (Vec2){x,4}));
			assert(ngon_contains_point((Vec2[]){B, C, A}, 3, (Vec2){x,4}));
			assert(ngon_contains_point((Vec2[]){C, A, B}, 3, (Vec2){x,4}));
			assert(ngon_contains_point((Vec2[]){C, B, A}, 3, (Vec2){x,4}));
		} else {
			assert(!ngon_contains_point((Vec2[]){A, B, C}, 3, (Vec2){x,4}));
			assert(!ngon_contains_point((Vec2[]){A, C, B}, 3, (Vec2){x,4}));
			assert(!ngon_contains_point((Vec2[]){B, A, C}, 3, (Vec2){x,4}));
			assert(!ngon_contains_point((Vec2[]){B, C, A}, 3, (Vec2){x,4}));
			assert(!ngon_contains_point((Vec2[]){C, A, B}, 3, (Vec2){x,4}));
			assert(!ngon_contains_point((Vec2[]){C, B, A}, 3, (Vec2){x,4}));
		}
	}
}
