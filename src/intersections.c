#include "intersections.h"

static bool which_side_of_line(Vec2 start, Vec2 end, Vec2 point)
{
	Vec2 diff = vec2_sub(end, start);
	Vec2 v = vec2_sub(point, start);
	return mat2_det((Mat2){ .rows={
		{ diff.x, v.x },
		{ diff.y, v.y },
	}}) >= 0;
}

// not static, because tests
bool ngon_contains_point(const Vec2 *corners, int n, const Vec2 point)
{
	for (int i = 0; i < n; i++) {
		Vec2 start = corners[(i+1) % n];
		Vec2 end = corners[(i+2) % n];
		// TODO: assuming convexity, which_side_of_line(start, end, corners[i]) is always the same boolean
		if (which_side_of_line(start, end, point) != which_side_of_line(start, end, corners[i]))
			return false;
	}
	return true;
}

// assumes convexity of outer n-gon
static bool ngon_contains_ngon(const Vec2 *outer, const Vec2 *inner, int n)
{
	for (int i = 0; i < n; i++)
		if (!ngon_contains_point(outer, n, inner[i]))
			return false;
	return true;
}

bool intersect_tetragons(const Vec2 *tetra1, const Vec2 *tetra2, Vec2 *ipoint)
{
	for (int i = 0; i < 4; i++)
		for (int k = 0; k < 4; k++)
			if (intersect_line_segments(tetra1[i], tetra1[(i+1)%4], tetra2[k], tetra2[(k+1)%4], false, ipoint))
				return true;

	// one tetragon can be nested inside the other
	if (ngon_contains_ngon(tetra1, tetra2, 4)) {
		*ipoint = tetra2[0];
		return true;
	}
	if (ngon_contains_ngon(tetra2, tetra1, 4)) {
		*ipoint = tetra1[0];
		return true;
	}
	return false;
}
