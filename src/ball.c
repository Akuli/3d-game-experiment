#include "ball.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stb_image.h>
#include <stb_image_resize.h>
#include "camera.h"
#include "common.h"
#include "mathstuff.h"

#define IS_TRANSPARENT(color) ((color).a < 0x80)

static SDL_Color average_color(SDL_Color *pixels, size_t npixels)
{
	// yes, rgb math is bad ikr
	unsigned long long rsum = 0, gsum = 0, bsum = 0;
	size_t count = 0;

	for (size_t i = 0; i < npixels; i += 4) {
		if (IS_TRANSPARENT(pixels[i]))
			continue;

		rsum += pixels[i].r;
		gsum += pixels[i].g;
		bsum += pixels[i].b;
		count++;
	}

	if (count == 0) {
		// just return something, avoid divide by zero
		return (SDL_Color){ 0xff, 0xff, 0xff, 0xff };
	}

	return (SDL_Color){
		(uint8_t) iclamp((int)(rsum / count), 0, 0xff),
		(uint8_t) iclamp((int)(gsum / count), 0, 0xff),
		(uint8_t) iclamp((int)(bsum / count), 0, 0xff),
		0xff,
	};
}

static void replace_alpha_with_average(SDL_Color *pixels, size_t npixels)
{
	SDL_Color avg = average_color(pixels, npixels);

	for (size_t i = 0; i < npixels; i += 1) {
		if (IS_TRANSPARENT(pixels[i]))
			pixels[i] = avg;
		pixels[i].a = 0xff;
	}
}

static void read_image(const char *filename, SDL_Color *res)
{
	FILE *f = fopen(filename, "rb");
	if (!f)
		fatal_error_printf("fopen failed: %s", strerror(errno));

	/*
	On a "typical" system, we can convert between SDL_Color arrays and uint8_t
	arrays with just casts.
	*/
	static_assert(sizeof(SDL_Color) == 4, "weird padding");
	static_assert(offsetof(SDL_Color, r) == 0, "weird structure order");
	static_assert(offsetof(SDL_Color, g) == 1, "weird structure order");
	static_assert(offsetof(SDL_Color, b) == 2, "weird structure order");
	static_assert(offsetof(SDL_Color, a) == 3, "weird structure order");

	int chansinfile, filew, fileh;
	SDL_Color *filedata = (SDL_Color*) stbi_load_from_file(f, &filew, &fileh, &chansinfile, 4);
	fclose(f);
	if (!filedata)
		fatal_error_printf("stbi_load_from_file failed: %s", stbi_failure_reason());

	replace_alpha_with_average(filedata, (size_t)filew*(size_t)fileh);

	int ok = stbir_resize_uint8(
		(uint8_t*) filedata, filew, fileh, 0,
		(uint8_t*) res, BALL_PIXELS_AROUND, BALL_PIXELS_VERTICALLY, 0,
		4);
	stbi_image_free(filedata);
	if (!ok)
		fatal_error_printf("stbir_resize_uint8 failed: %s", stbi_failure_reason());
}

struct Ball *ball_load(const char *filename, Vec3 center)
{
	struct Ball *ball = malloc(sizeof(*ball));
	if (!ball)
		fatal_error("not enough memory");

	ball->center = center;
	read_image(filename, (SDL_Color*) ball->image);
	ball->transform = (Mat3){ .rows = {
		{1, 0, 0},
		{0, 1, 0},
		{0, 0, 1},
	}};
	ball->transform_inverse = mat3_inverse(ball->transform);
	return ball;
}

/*
We introduce third type of coordinates: untransformed ball coordinates. These
coordinates have (0,0,0) as ball center, and ball->transform and cam->world2cam
haven't been applied yet.
*/

// +1 for vertical because we want to include both ends
// no +1 for the other one because it wraps around instead
typedef Vec3 VectorArray[BALL_PIXELS_VERTICALLY + 1][BALL_PIXELS_AROUND];

/*
Where on the ball's surface will each pixel go? This function calculates vectors
so that we don't need to call slow trig functions every time the ball is drawn.
Returns untransformed ball coordinates.
*/
static const VectorArray *get_untransformed_surface_vectors(void)
{
	static VectorArray res;
	static bool ready = false;

	if (ready)
		return (const VectorArray *) &res;

	float pi = acosf(-1);

	for (size_t v = 0; v < BALL_PIXELS_VERTICALLY+1; v++) {
		float y = BALL_RADIUS - 2*BALL_RADIUS*(float)v/BALL_PIXELS_VERTICALLY;
		float xzrad = sqrtf(BALL_RADIUS*BALL_RADIUS - y*y);  // radius on xz plane

		for (size_t a = 0; a < BALL_PIXELS_AROUND; a++) {
			float angle = (float)a/BALL_PIXELS_AROUND * 2*pi;
			float x = xzrad*sinf(angle);
			float z = xzrad*cosf(angle);
			res[v][a] = (Vec3){ x, y, z };
		}
	}

	ready = true;
	return (const VectorArray *) &res;
}

/*
A part of the ball is visible to the camera. The rest isn't. The plane returned
by this function divides the ball into the visible part and the part behind the
visible part. The normal vector of the plane points toward the visible side, so
plane_whichside() returns whether a point on the ball is visible.

The returned plane is in untransformed ball coordinates.

This assumes that the ball is round in both coordinates, even though it might not
be the case. This still seems to be "close enough".
*/
static struct Plane
get_visibility_plane(const struct Ball *ball, const struct Camera *cam)
{
	/*
	Calculate camera location in untransformed ball coordinates. This must work
	so that once the resulting camera vector is
		1. transformed with ball->transform
		2. transformed with cam->world2cam
		3. added with ball->center
	then we get the camera location in camera coordinates, i.e. (0,0,0).
	*/
	Vec3 cam2center = camera_point_world2cam(cam, ball->center);
	Vec3 center2cam = vec3_neg(cam2center);
	vec3_apply_matrix(&center2cam, mat3_inverse(cam->world2cam));
	vec3_apply_matrix(&center2cam, ball->transform_inverse);

	/*
	From the side, the ball being split by the visibility plane looks like this:

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

	Note that the plane is closer to the camera than the ball center. The center
	is marked with o above. Note that we are using untransformed ball coordinates,
	so we have o=(0,0,0).

	Let D denote the distance between visibility plane and the ball center. With
	similar triangles and Pythagorean theorem, we get

		D = BALL_RADIUS^2/|center2cam|.

	The equation of the plane is

		projection of (x,y,z) onto center2cam = D,

	because center2cam is a normal vector of the plane. By writing the projection
	with dot product, we get

		((x,y,z) dot center2cam) / |center2cam| = D.

	This simplifies:

		(x,y,z) dot center2cam = BALL_RADIUS^2
	*/
	return (struct Plane) {
		.normal = center2cam,
		.constant = BALL_RADIUS*BALL_RADIUS,
	};
}

void ball_display(struct Ball *ball, const struct Camera *cam)
{
	// ball center vector as camera coordinates
	Vec3 center = camera_point_world2cam(cam, ball->center);

	// vplane is in untransformed ball coordinates
	struct Plane vplane = get_visibility_plane(ball, cam);

	/*
	To convert from untransformed ball coordinates to camera coordinates, apply
	this and then add the ball center vector as camera coordinates
	*/
	Mat3 ball2cam = mat3_mul_mat3(cam->world2cam, ball->transform);

	const VectorArray *usvecs = get_untransformed_surface_vectors();
	for (size_t v = 0; v < BALL_PIXELS_VERTICALLY + 1; v++) {
		for (size_t a = 0; a < BALL_PIXELS_AROUND; a++) {
			// this is perf critical code
			// turns out that in-place operations are measurably faster

			ball->sidecache[v][a] = plane_whichside(vplane, (*usvecs)[v][a]);

			ball->vectorcache[v][a] = (*usvecs)[v][a];
			vec3_apply_matrix(&ball->vectorcache[v][a], ball2cam);
			vec3_add_inplace(&ball->vectorcache[v][a], center);
		}
	}

	for (size_t a = 0; a < BALL_PIXELS_AROUND; a++) {
		size_t a2 = (a+1) % BALL_PIXELS_AROUND;

		for (size_t v = 0; v < BALL_PIXELS_VERTICALLY; v++) {
			size_t v2 = v+1;

			// this is perf critical code

			if (!ball->sidecache[v][a] &&
				!ball->sidecache[v][a2] &&
				!ball->sidecache[v2][a] &&
				!ball->sidecache[v2][a2])
			{
				continue;
			}

			SDL_Rect rect;
			if (camera_get_containing_rect(
					cam, &rect,
					ball->vectorcache[v][a],
					ball->vectorcache[v][a2],
					ball->vectorcache[v2][a],
					ball->vectorcache[v2][a2]))
			{
				SDL_FillRect(
					cam->surface, &rect,
					convert_color(cam->surface, ball->image[v][a]));
			}
		}
	}
}

bool ball_intersect_line(const struct Ball *ball, struct Line ln, Vec3 *res1, Vec3 *res2)
{
	// switch to coordinates with ball->transform unapplied
	vec3_apply_matrix(&ln.dir, ball->transform_inverse);
	vec3_apply_matrix(&ln.point, ball->transform_inverse);
	Vec3 center = mat3_mul_vec3(ball->transform_inverse, ball->center);

	float distSQUARED = line_point_distanceSQUARED(ln, center);
	if (distSQUARED > BALL_RADIUS*BALL_RADIUS)
		return false;

	// now we are entering a less common case, and calculating sqrt isn't too bad

	Vec3 line2center = vec3_sub(center, ln.point);
	Vec3 line2mid = vec3_project(line2center, ln.dir);
	Vec3 mid = vec3_add(ln.point, line2mid);

	// pythagorean theorem gives distance along line
	float linedist = sqrtf(BALL_RADIUS*BALL_RADIUS - distSQUARED);
	Vec3 mid2res = vec3_withlength(ln.dir, linedist);
	*res1 = vec3_add(mid, mid2res);
	*res2 = vec3_sub(mid, mid2res);

	vec3_apply_matrix(res1, ball->transform);
	vec3_apply_matrix(res2, ball->transform);
	return true;
}
