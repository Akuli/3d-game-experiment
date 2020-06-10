#include "player.h"
#include <assert.h>
#include <math.h>
#include "ellipsoid.h"
#include "log.h"
#include "mathstuff.h"
#include "sound.h"
#include "wall.h"

#define MOVE_UNITS_PER_SECOND 8.f
#define RADIANS_PER_SECOND 5.0f

#define CAMERA_BEHIND_PLAYER 4.0f
#define CAMERA_HEIGHT 2.0f

#define JUMP_MAX_HEIGHT 3.0f
#define JUMP_DURATION_SEC 0.6f

static float get_jump_height(unsigned int jumpframe, unsigned int fps)
{
	float time = (float)jumpframe / (float)fps;

	/*
	Parabola that intersects time axis at time=0 and time=JUMP_DURATION_SEC, having
	max value of JUMP_MAX_HEIGHT
	*/
	float a = (-4*JUMP_MAX_HEIGHT)/(JUMP_DURATION_SEC*JUMP_DURATION_SEC);
	return a*(time - 0)*(time - JUMP_DURATION_SEC);
}

static float get_y_radius(const struct Player *plr, unsigned int fps)
{
	if (plr->flat)   // if flat and jumping, then do this
		return PLAYER_HEIGHT_FLAT / 2;

	return 1.5f*plr->ellipsoid.xzradius + 0.3f*get_jump_height(plr->jumpframe, fps);
}

void player_eachframe(struct Player *plr, unsigned int fps, const struct Wall *walls, size_t nwalls)
{
	if (plr->turning != 0) {
		float pi = acosf(-1);
		plr->ellipsoid.angle += (RADIANS_PER_SECOND / (float)fps) * (float)plr->turning;
		plr->ellipsoid.angle = fmodf(plr->ellipsoid.angle, 2*pi);
		// ellipsoid_update_transforms() called below
	}

	if (plr->moving) {
		Mat3 cam2world = mat3_inverse(plr->cam.world2cam);
		Vec3 diff = mat3_mul_vec3(
			cam2world,
			(Vec3){ 0, 0, -MOVE_UNITS_PER_SECOND/(float)fps });
		plr->ellipsoid.center = vec3_add(plr->ellipsoid.center, diff);
	}

	float y = 0;
	if (plr->jumpframe != 0) {
		plr->jumpframe++;
		y = get_jump_height(plr->jumpframe, fps);
		if (y < 0) {
			// land
			plr->jumpframe = 0;
			y = 0;
		}
	}

	plr->ellipsoid.xzradius = 0.3f;
	plr->ellipsoid.yradius = get_y_radius(plr, fps);
	ellipsoid_update_transforms(&plr->ellipsoid);

	plr->ellipsoid.center.y = y + plr->ellipsoid.yradius;

	for (size_t i = 0; i < nwalls; i++)
		wall_bumps_ellipsoid(&walls[i], &plr->ellipsoid);

	Vec3 diff = { 0, 0, CAMERA_BEHIND_PLAYER };
	vec3_apply_matrix(&diff, mat3_rotation_xz(plr->ellipsoid.angle));

	plr->cam.angle = plr->ellipsoid.angle;
	plr->cam.location = vec3_add(plr->ellipsoid.center, diff);
	plr->cam.location.y = CAMERA_HEIGHT;

	camera_update_caches(&plr->cam);
}

void player_set_turning(struct Player *plr, int dir, bool turn)
{
	assert(dir == 1 || dir == -1);

	if (turn)
		plr->turning = dir;
	else if (plr->turning == dir)
		plr->turning = 0;
}

void player_set_moving(struct Player *plr, bool mv)
{
	plr->moving = mv;
}

void player_set_flat(struct Player *plr, bool flat)
{
	if (plr->flat == flat)
		return;

	plr->flat = flat;

	if (plr->flat)
		sound_play("lemonsqueeze.wav");
	else {
		sound_play("pop.wav");
		if (plr->jumpframe == 0) {
			sound_play("boing.wav");
			plr->jumpframe = 1;
		}
	}
}
