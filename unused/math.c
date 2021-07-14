/*
Where on the ellipsoid's surface will each pixel go? This function calculates vectors
so that we don't need to call slow trig functions every time an ellipsoid is drawn.
Returns unit ball coordinates.
*/
static const VectorArray *get_untransformed_surface_vectors(void)
{
	static VectorArray res;
	static bool ready = false;

	if (ready)
		return (const VectorArray *) &res;

	float pi = acosf(-1);

	for (size_t v = 0; v < ELLIPSOID_PIXELS_VERTICALLY+1; v++) {
		float y = 1 - 2*(float)v/ELLIPSOID_PIXELS_VERTICALLY;
		float xzrad = sqrtf(1 - y*y);  // radius on xz plane

		for (size_t a = 0; a < ELLIPSOID_PIXELS_AROUND; a++) {
			/*
			+pi sets the angle of the back of the player, corresponding to a=0.
			This way, the player looks into the angle=0 direction.
			Minus sign is needed to avoid mirror imaging the pic for some reason.
			*/
			float angle = -(float)a/ELLIPSOID_PIXELS_AROUND * 2*pi + pi;
			float x = xzrad*cosf(angle);
			float z = xzrad*sinf(angle);
			res[v][a] = (Vec3){ x, y, z };
		}
	}

	ready = true;
	return (const VectorArray *) &res;
}

/*
A part of the ellipsoid is visible to the camera. The rest isn't. The plane returned
by this function splits the ellipsoid into the visible part and the part behind the
visible part. The normal vector of the plane points toward the visible side, so
plane_whichside() returns whether a point on the ellipsoid is visible.

The returned plane is in unit ball coordinates.
*/
static struct Plane
get_splitter_plane(const struct Ellipsoid *el, const struct Camera *cam)
{
	/*
	Calculate camera location in unit ball coordinates. This must work
	so that once the resulting camera vector is
		1. transformed with el->transform
		2. transformed with cam->world2cam
		3. added with el->center
	then we get the camera location in camera coordinates, i.e. (0,0,0).
	*/
	Vec3 cam2center = camera_point_world2cam(cam, el->center);
	Vec3 center2cam = vec3_neg(cam2center);
	vec3_apply_matrix(&center2cam, mat3_inverse(cam->world2cam));
	vec3_apply_matrix(&center2cam, el->transform_inverse);

	/*
	From the side, the el being split by the visibility plane looks like this:

        \  /
         \/___
         /\   \
        /| \o  |
	   /  \_\_/
    cam^^^^^^\^^^^^^^
              \
               \
	       visibility
	         plane

	Note that the plane is closer to the camera than the el center. The center
	is marked with o above. Note that we are using unit ball coordinates,
	so we have o=(0,0,0).

	Let D denote the distance between visibility plane and the el center. With
	similar triangles and Pythagorean theorem, we get

		D = 1/|center2cam|,

	where 1 = 1^2 = (unit ball radius)^2. The equation of the plane is

		projection of (x,y,z) onto center2cam = D,

	because center2cam is a normal vector of the plane. By writing the projection
	with dot product, we get

		((x,y,z) dot center2cam) / |center2cam| = D.

	This simplifies:

		(x,y,z) dot center2cam = 1
	*/
	return (struct Plane) {
		.normal = center2cam,
		.constant = 1,
	};
}

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

/*
If x is a solution of

	coeffs[0] + coeffs[1]*x + ... + coeffs[n]*x^n = 0,

then x is not greater than the return value of this function.
*/
static float polynomial_root_upper_bound(const float *coeffs, unsigned n)
{
	// Cauchy's bounds
	// https://en.wikipedia.org/wiki/Geometrical_properties_of_polynomial_roots#Lagrange's_and_Cauchy's_bounds
	float res = 1;
	for (unsigned i = 0; i < n; i++)
		res = max(res, fabsf(coeffs[i]/coeffs[n]));
	return res;
}

/*
If x is a solution of

	coeffs[0] + coeffs[1]*x + ... + coeffs[n]*x^n = 0,

then x is not greater than the return value of this function. Never returns a
negative value.
*/
static float polynomial_root_upper_bound(const float *coeffs, unsigned n)
{
	// Cauchy's bounds
	// https://en.wikipedia.org/wiki/Geometrical_properties_of_polynomial_roots#Lagrange's_and_Cauchy's_bounds
	float res = 1;
	for (unsigned i = 0; i < n; i++)
		res = max(res, fabsf(coeffs[i]/coeffs[n]));
	return res;
}

/*
Finds a roots of

	coeffs[0] + coeffs[1]*x + coeffs[2]*x^2 + coeffs[3]*x^3 + coeffs[4]*x^4 = 0

near the given guess (lol).
*/
static float find_degree4_polynomial_root(const float *coeffs, float guess)
{
	assert(coeffs[4] != 0);

	// divide both sides by coeffs[4]  -->  x^4+ax^3+bx^2+cx+d = 0
	float a = coeffs[3] / coeffs[4];
	float b = coeffs[2] / coeffs[4];
	float c = coeffs[1] / coeffs[4];
	float d = coeffs[0] / coeffs[4];

	float x = guess;
	float closenessreq = polynomial_root_upper_bound(coeffs, 4) / 100000;

	int iter = 0;
	float sub;
	do {
		// newton's method: x_(n+1) = x_n - f(x_n)/f'(x_n)
		float fval = x*x*x*x + a*x*x*x + b*x*x + c*x + d;
		float derivative = 4*x*x*x + 3*a*x*x + 2*b*x + c;

		// this doesn't behave nicely if derivative is zero or small
		sub = fval / derivative;
		assert(isfinite(sub));
		x -= sub;

		if (++iter == 50) {
			nonfatal_error("hitting max number of iterations");
			break;
		}
	} while (fabsf(sub) > closenessreq);

	return x;
}

// Find the distance between ellipse (x/a)^2 + (y/b)^2 = 1 and point
static float smallest_distance_between_ellipse_and_point(float a, float b, Vec2 pt)
{
	assert(a > 0);
	assert(b > 0);

	/*
	We parametrize the ellipse as (x,y) = E(t), where

		E(t) = (a cos(t), b sin(t)).

	As t goes from 0 to 2pi, this rotates around the ellipse counter-clockwise.
	With a=b=1, this is the high-school unit circle, and here a and b are just
	stretching that.

	We want to find the point on the ellipse closest to pt. The vector from it to
	pt is perpendicular to the ellipse, because otherwise we find a nearby point
	with an even smaller distance to pt. A vector going counter-clockwise along
	the ellipse is given by

		E'(t) = (-a sin(t), b cos(t)).

	If we rotate this 90 degrees, we get a vector perpendicular to the ellipse.
	Consider the rotation

		rotate90clockwise(x,y) = (y, -x).

	The vector

		A(t) = rotate90clockwise(E'(t)) = (b cos(t), a sin(t))

	is pointing away from the ellipse perpendicularly. Now we should have

		E(t) + m A(t) = (pt.x, pt.y)

	for some unknown positive number m. Plugging in and rewriting gives

		(a + mb)(b + ma)(cos(t), sin(t)) = ((b + ma)pt.x, (a + mb)pt.y).

	This is nice because by comparing magnitudes of these vectors, we get rid of
	the unknown t. Doing that gives

		(a + mb)(b + ma) = sqrt( (b + ma)^2 pt.x^2 + (a + mb)^2 pt.y^2 ).

	Because neither side is negative, we can ^2 both sides and "simplify" to get

		coeffs[4]*m^4 + coeffs[3]*m^3 + coeffs[2]*m^2 + coeffs[1]*m + coeffs[0] = 0

	with the following coeffs.
	*/
	float coeffs[] = {
		[4] = a*a*b*b,
		[3] = 2*a*b*(a*a + b*b),
		[2] = (a*a*a*a + b*b*b*b) + 4*a*a*b*b - (a*a*pt.x*pt.x + b*b*pt.y*pt.y),
		[1] = 2*a*b*(a*a + b*b - pt.x*pt.x - pt.y*pt.y),
		[0] = a*a*b*b - a*a*pt.y*pt.y - b*b*pt.x*pt.x,
	};

	// Above we saw that the only positive solution should be our m.
	float m = degree4_polynomial_biggest_root(coeffs);
	assert(m > 0);

	/*
	We want to calculate |m A(t)|, and for that we need cos(t) and sin(t). Above
	we got

		(a + mb)(b + ma)(cos(t), sin(t)) = ((b + ma)pt.x, (a + mb)pt.y).
	*/
	float cost = pt.x / (a + m*b);
	float sint = pt.y / (b + m*a);

	return hypotf(m * b * cost, m * a * sint);
}
