#include "player.h"
#include <assert.h>
#include <math.h>
#include "ellipsoid.h"
#include "log.h"
#include "mathstuff.h"
#include "sound.h"
#include "wall.h"
#include "../generated/filelist.h"

#define MOVE_UNITS_PER_SECOND 8.f
#define RADIANS_PER_SECOND 5.0f

#define CAMERA_BEHIND_PLAYER 4.0f
#define CAMERA_HEIGHT 2.0f

#define JUMP_MAX_HEIGHT 3.0f
#define JUMP_DURATION_SEC 0.6f


const struct EllipsoidPic *player_get_epics(const SDL_PixelFormat *fmt)
{
	static struct EllipsoidPic res[FILELIST_NPLAYERS];
	static const SDL_PixelFormat *cachedfmt = NULL;

	if (!cachedfmt) {
		assert(fmt != NULL);
		cachedfmt = fmt;
		// this loop can cause slow startup time
		for (int i = 0; i < FILELIST_NPLAYERS; i++)
			ellipsoidpic_load(&res[i], filelist_players[i], fmt);
	}

	assert(fmt == NULL || fmt == cachedfmt);
	return res;
}

const char *player_getname(const struct EllipsoidPic *epic)
{
	int i = epic - player_get_epics(NULL);
	assert(0 <= i && i < FILELIST_NPLAYERS);
	const char *path = filelist_players[i];

	const char *prefix = "players/";
	assert(strstr(path, prefix) == path);
	path += strlen(prefix);

	static char name[100] = {0};
	strncpy(name, path, sizeof(name)-1);
	char *dot = strrchr(name, '.');
	assert(dot);
	*dot = '\0';

	return name;
}

static float get_jump_height(int jumpframe)
{
	float time = (float)jumpframe / (float)CAMERA_FPS;

	/*
	Parabola that intersects time axis at time=0 and time=JUMP_DURATION_SEC, having
	max value of JUMP_MAX_HEIGHT
	*/
	float a = (-4*JUMP_MAX_HEIGHT)/(JUMP_DURATION_SEC*JUMP_DURATION_SEC);
	return a*(time - 0)*(time - JUMP_DURATION_SEC);
}

static float get_y_radius(const struct Player *plr)
{
	if (plr->flat)   // if flat and jumping, then do this
		return PLAYER_HEIGHT_FLAT / 2;

	return PLAYER_YRADIUS_NOFLAT + 0.3f*get_jump_height(plr->jumpframe);
}

void player_eachframe(struct Player *plr, const struct Wall *walls, int nwalls)
{
	if (plr->turning != 0) {
		plr->ellipsoid.angle += (RADIANS_PER_SECOND / (float)CAMERA_FPS) * (float)plr->turning;
		// ellipsoid_update_transforms() called below
	}

	if (plr->moving) {
		Vec3 diff = mat3_mul_vec3(
			plr->cam.cam2world,
			(Vec3){ 0, 0, -MOVE_UNITS_PER_SECOND/(float)CAMERA_FPS });
		vec3_add_inplace(&plr->ellipsoid.center, diff);
	}

	float y = 0;
	if (plr->jumpframe != 0) {
		plr->jumpframe++;
		y = get_jump_height(plr->jumpframe);
		if (y < 0) {
			// land
			plr->jumpframe = 0;
			y = 0;
		}
	}

	plr->ellipsoid.xzradius = PLAYER_XZRADIUS;
	plr->ellipsoid.yradius = get_y_radius(plr);
	ellipsoid_update_transforms(&plr->ellipsoid);

	plr->ellipsoid.center.y = y + plr->ellipsoid.yradius;

	for (int i = 0; i < nwalls; i++)
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
