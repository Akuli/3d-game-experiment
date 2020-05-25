#include "player.h"
#include <assert.h>
#include "ball.h"
#include "mathstuff.h"

#define MOVE_UNITS_PER_SECOND 10.f
#define RADIANS_PER_SECOND 5.0f

#define CAMERA_BEHIND_PLAYER 3.0f
#define CAMERA_HEIGHT 2.0f

#define GRAVITY 0.03f
#define INITIAL_VELOCITY 0.5f

static float get_stretch(const struct Player *plr)
{
	if (plr->flat)   // if flat and jumping, then do this
		return 0.2f;
	if (plr->jumping)
		return 3;
	return 1.5f;
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

	if (plr->jumping) {
		plr->yvelocity -= GRAVITY;
		plr->ball->center.y += plr->yvelocity;
		if (plr->yvelocity < 0 && plr->ball->center.y < BALL_RADIUS*get_stretch(plr)) {
			// land
			plr->jumping = false;   // changes get_stretch() value
			plr->ball->center.y = BALL_RADIUS*get_stretch(plr);
		}
	} else {
		// make sure it touches ground, ignore yvelocity
		plr->ball->center.y = BALL_RADIUS*get_stretch(plr);
	}

	Mat3 rot = mat3_rotation_xz(plr->angle);
	Mat3 antirot = mat3_rotation_xz(-plr->angle);

	plr->ball->transform = mat3_mul_mat3(rot, (Mat3){.rows= {
		{ 1, 0, 0 },
		{ 0, get_stretch(plr), 0 },
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

	if (!plr->jumping) {
		if (plr->flat) {
			// make sure that player stays touching the ground
			plr->ball->center.y = BALL_RADIUS*get_stretch(plr);
		} else {
			// jump
			plr->jumping = true;
			plr->ball->center.y += get_stretch(plr)*BALL_RADIUS;
			plr->yvelocity = INITIAL_VELOCITY;
		}
	}
}
