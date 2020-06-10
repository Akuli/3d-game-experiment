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

struct Enemy {
	struct Ellipsoid ellipsoid;
	bool turning;
	enum EnemyDir dir;
};

// run this after creating a new enemy
void enemy_init(struct Enemy *en, const struct EllipsoidPic *epic);

// runs fps times per second
void enemy_eachframe(struct Enemy *en, unsigned int fps, const struct Place *pl);


#endif   // ENEMY_H
