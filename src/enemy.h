#ifndef ENEMY_H
#define ENEMY_H

#include <SDL2/SDL.h>
#include "ellipsoid.h"
#include "place.h"

#define ENEMY_XZRADIUS 0.45f
#define ENEMY_YRADIUS  1.2f

enum EnemyDir {
	ENEMY_DIR_XPOS,
	ENEMY_DIR_XNEG,
	ENEMY_DIR_ZPOS,
	ENEMY_DIR_ZNEG,
};

enum EnemyFlags {
	ENEMY_STUCK = 0x01,     // can't move anywhere, so just spin without changing ellipsoid.center
	ENEMY_TURNING = 0x02,   // soon will be looking into enemy->dir direction
	ENEMY_NEVERDIE = 0x04,  // see place.c for description
};

struct Enemy {
	const struct Place *place;
	struct Ellipsoid ellipsoid;
	enum EnemyFlags flags;
	enum EnemyDir dir;
};

// call enemy_init_epics() once before calling enemy_new() as many times as you like
void enemy_init_epics(const SDL_PixelFormat *fmt);
struct Enemy enemy_new(const struct Place *pl, enum EnemyFlags fl);

// for place editor
const struct EllipsoidPic *enemy_getfirstepic(void);

// runs fps times per second for each enemy
void enemy_eachframe(struct Enemy *en);


#endif   // ENEMY_H
