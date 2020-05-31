/*
functions that could be useful if you change how stuff works, but
currently not needed
*/

// Find intersection point of wall and line, return false if no intersection
bool wall_intersect_line(const struct Wall *w, struct Line ln, Vec3 *res)
{
	float number;

	switch(w->dir) {
	case WALL_DIR_XY:
		/*
		plane equation: z = w->startz
		line equation: (x,y,z) = ln.point + number*ln.dir

		Comparing z on both sides of line equation gives the unknown number.
		Then we can use the number to calculate x and y.
		*/
		number = ((float)w->startz - ln.point.z)/ln.dir.z;
		res->x = ln.point.x + number*ln.dir.x;
		res->y = ln.point.y + number*ln.dir.y;
		res->z = (float)w->startz;
		return Y_MIN < res->y && res->y < Y_MAX &&
			(float)w->startx < res->x && res->x < (float)(w->startx + 1);

	case WALL_DIR_ZY:
		number = ((float)w->startx - ln.point.x)/ln.dir.x;
		res->x = (float)w->startx;
		res->y = ln.point.y + number*ln.dir.y;
		res->z = ln.point.z + number*ln.dir.z;
		return Y_MIN < res->y && res->y < Y_MAX &&
			(float)w->startz < res->z && res->z < (float)(w->startz + 1);
	}

	return false;   // never runs, make compiler happy
}

/*
Find intersection point of ball and line, returning false if no intersection.

Typically the line enters the ball somewhere and exits the ball at some other
point. Those are the two result pointers that this function takes.
*/
bool ball_intersect_line(const struct Ball *ball, struct Line ln, Vec3 *res1, Vec3 *res2)
{
	// switch to coordinates with ball->transform unapplied. Will have radius 1.
	vec3_apply_matrix(&ln.dir, ball->transform_inverse);
	vec3_apply_matrix(&ln.point, ball->transform_inverse);
	Vec3 center = mat3_mul_vec3(ball->transform_inverse, ball->center);

	float distSQUARED = line_point_distanceSQUARED(ln, center);
	if (distSQUARED > 1)
		return false;

	// now we are entering a less common case, and calculating sqrt isn't too bad

	Vec3 line2center = vec3_sub(center, ln.point);
	Vec3 line2mid = vec3_project(line2center, ln.dir);
	Vec3 mid = vec3_add(ln.point, line2mid);

	// pythagorean theorem gives distance along line
	float linedist = sqrtf(1 - distSQUARED);
	Vec3 mid2res = vec3_withlength(ln.dir, linedist);
	*res1 = vec3_add(mid, mid2res);
	*res2 = vec3_sub(mid, mid2res);

	vec3_apply_matrix(res1, ball->transform);
	vec3_apply_matrix(res2, ball->transform);
	return true;
}

// Return a plane that the wall is a part of
struct Plane wall_getplane(const struct Wall *w)
{
	switch(w->dir) {
	case WALL_DIR_XY:
		// plane equation: z = w->startz
		return (struct Plane){ .normal = {0,0,1}, .constant = (float)w->startz };
	case WALL_DIR_ZY:
		return (struct Plane){ .normal = {1,0,0}, .constant = (float)w->startx };
	}

	return (struct Plane){0};    // never runs, make compiler happy
}
