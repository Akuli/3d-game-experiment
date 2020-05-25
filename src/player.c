#include "player.h"
#include "ball.h"
#include "mathstuff.h"

#define MOVE_UNITS_PER_SECOND 10.f
#define RADIANS_PER_SECOND 5.0f

#define CAMERA_BEHIND_PLAYER 3.0f
#define CAMERA_ABOVE_PLAYER 1.0f

void player_updatecam(struct Player *plr)
{
	plr->cam.world2cam = mat3_rotation_xz(-plr->ball->angle);

	Vec3 diff = mat3_mul_vec3(
		mat3_inverse(plr->cam.world2cam),
		(Vec3){ 0, CAMERA_ABOVE_PLAYER, CAMERA_BEHIND_PLAYER });
	plr->cam.location = vec3_add(plr->ball->center, diff);
}

void player_turn(struct Player *plr, unsigned int fps)
{
	if (plr->turning != 0) {
		plr->ball->angle += (RADIANS_PER_SECOND / (float)fps) * (float)plr->turning;
		player_updatecam(plr);
	}
}

void player_move(struct Player *plr, unsigned int fps)
{
	if (!plr->moving)
		return;

	Vec3 diff = mat3_mul_vec3(
		mat3_inverse(plr->cam.world2cam),
		(Vec3){ 0, 0, -MOVE_UNITS_PER_SECOND/(float)fps });
	plr->ball->center = vec3_add(plr->ball->center, diff);
	player_updatecam(plr);
}
