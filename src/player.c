#include "player.h"
#include <assert.h>
#include "ball.h"
#include "common.h"
#include "mathstuff.h"
#include "sound.h"

#define MOVE_UNITS_PER_SECOND 10.f
#define RADIANS_PER_SECOND 5.0f

#define CAMERA_BEHIND_PLAYER 3.0f
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

static float get_stretch(const struct Player *plr, unsigned int fps)
{
	if (plr->flat)   // if flat and jumping, then do this
		return 0.2f;
	return 1.5f + get_jump_height(plr->jumpframe, fps);
}

void player_eachframe(struct Player *plr, unsigned int fps)
{
	if (plr->turning != 0)
		plr->angle += (RADIANS_PER_SECOND / (float)fps) * (float)plr->turning;

	if (plr->moving) {
		Vec3 diff = mat3_mul_vec3(
			mat3_inverse(plr->cam.world2cam),
			(Vec3){ 0, 0, -MOVE_UNITS_PER_SECOND/(float)fps });
		plr->ball->center = vec3_add(plr->ball->center, diff);
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

	float stretch = get_stretch(plr, fps);
	plr->ball->center.y = y + BALL_RADIUS*stretch;

	Mat3 rot = mat3_rotation_xz(plr->angle);
	Mat3 antirot = mat3_rotation_xz(-plr->angle);

	plr->ball->transform = mat3_mul_mat3(rot, (Mat3){.rows= {
		{ 1, 0, 0 },
		{ 0, stretch, 0 },
		{ 0, 0, 1 },
	}});
	plr->cam.world2cam = antirot;

	Vec3 diff = { 0, 0, CAMERA_BEHIND_PLAYER };
	vec3_apply_matrix(&diff, rot);
	plr->cam.location = vec3_add(plr->ball->center, diff);
	plr->cam.location.y = CAMERA_HEIGHT;
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
