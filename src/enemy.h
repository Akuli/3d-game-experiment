#ifndef ENEMY_H
#define ENEMY_H

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
	struct Ellipsoid ellipsoid;
	enum EnemyFlags flags;
	enum EnemyDir dir;
};

// run this after creating a new enemy
void enemy_init(struct Enemy *en, const SDL_PixelFormat *fmt, const struct Place *pl);

// runs fps times per second
void enemy_eachframe(struct Enemy *en, const struct Place *pl);


#endif   // ENEMY_H
