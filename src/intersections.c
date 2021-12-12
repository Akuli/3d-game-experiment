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


bool intersect_line_segments(Vec2 start1, Vec2 end1, Vec2 start2, Vec2 end2, bool infinite2, Vec2 *res)
{
	Vec2 dir1 = vec2_sub(end1, start1);
	Vec2 dir2 = vec2_sub(end2, start2);

	float dirdet = dir1.x*dir2.y - dir2.x*dir1.y;
	if (fabsf(dirdet) < 1e-5) {
		// same direction
		if (vec2_dot(dir1, dir2) < 0) {
			dir2.x *= -1;
			dir2.y *= -1;
			Vec2 tmp = start2;
			start2 = end2;
			end2 = tmp;
		}

		Vec2 perpdir = { dir1.y, -dir1.x };
		if (fabsf(vec2_dot(perpdir, start1) - vec2_dot(perpdir, start2)) > 1e-5f)
			return false;  // far apart
		if (infinite2)
			return true;

		// proj(v) = (projection of v onto dir1)*length(dir1). Length doesn't affect anything.
		#define proj(v) vec2_dot(dir1, (v))
		Vec2 olapstart = (proj(start1) < proj(start2)) ? start2 : start1;
		Vec2 olapend = (proj(end1) < proj(end2)) ? end1 : end2;
		if (proj(olapstart) >= proj(olapend))
			return false;
		#undef proj
		*res = vec2_mul_float(vec2_add(olapstart, olapend), 0.5f);
		return true;
	}

	/*
	At intersection start1 + t*dir1 = start2 + u*dir2, with t,u in [0,1].
	Solving t and u gives a linear system of two equations:
		 _               _   _ _
		| dir1.x  -dir2.x | | t |
		|                 | |   | = start2 - start1
		|_dir1.y  -dir2.y_| |_u_|
	*/
	Vec2 rhs = vec2_sub(start2, start1);
	float t = (dir2.x*rhs.y - dir2.y*rhs.x) / -dirdet;
	if (!(0 <= t && t <= 1))
		return false;
	if (!infinite2) {
		float u = (dir1.x*rhs.y - dir1.y*rhs.x) / -dirdet;
		if (!(0 <= u && u <= 1))
			return false;
	}
	*res = vec2_add(start1, vec2_mul_float(dir1, t));
	return true;
}
