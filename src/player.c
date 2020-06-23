#include "player.h"
#include "sphere.h"
#include "vecmat.h"

#define MOVE_UNITS_PER_SECOND 10.f
#define RADIANS_PER_SECOND 5.0f

#define CAMERA_BEHIND_PLAYER 3.0f
#define CAMERA_ABOVE_PLAYER 1.0f

static void update_camera_location(struct Player *plr)
{
	struct Vec3 diff = mat3_mul_vec3(
		mat3_inverse(plr->cam.world2cam),
		(struct Vec3){ 0, CAMERA_ABOVE_PLAYER, CAMERA_BEHIND_PLAYER });
	plr->cam.location = vec3_add(plr->sphere->center, diff);
}

void player_turn(struct Player *plr, unsigned int fps)
{
	plr->sphere->angle += (RADIANS_PER_SECOND / (float)fps) * (float)plr->turning;

	plr->cam.world2cam = mat3_rotation_xz(-plr->sphere->angle);
	update_camera_location(plr);
}

void player_move(struct Player *plr, unsigned int fps)
{
	if (!plr->moving)
		return;

	struct Vec3 diff = mat3_mul_vec3(
		mat3_inverse(plr->cam.world2cam),
		(struct Vec3){ 0, 0, -MOVE_UNITS_PER_SECOND/(float)fps });
	plr->sphere->center = vec3_add(plr->sphere->center, diff);
	update_camera_location(plr);
}
