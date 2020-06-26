#include "play.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "ellipsoid.h"
#include "enemy.h"
#include "guard.h"
#include "log.h"
#include "looptimer.h"
#include "mathstuff.h"
#include "max.h"
#include "misc.h"
#include "place.h"
#include "player.h"
#include "showall.h"
#include "sound.h"
#include "wall.h"

// includes all the GameObjects that all players should see
struct GameState {
	const struct Place *place;
	const SDL_PixelFormat *pixfmt;

	struct Player players[2];
	struct Enemy enemies[MAX_ENEMIES];
	int nenemies;

	struct Ellipsoid unpicked_guards[MAX_UNPICKED_GUARDS];
	int n_unpicked_guards;

	unsigned thisframe;
	unsigned lastenemyframe, lastguardframe;
};

static bool time_to_do_something(unsigned *frameptr, unsigned thisframe, unsigned delay)
{
	// https://yarchive.net/comp/linux/unsigned_arithmetic.html
	if (thisframe - *frameptr > delay) {
		*frameptr += delay;
		return true;
	}
	return false;
}

static struct Enemy *add_enemy(struct GameState *gs, enum EnemyFlags fl)
{
	if (gs->nenemies >= MAX_ENEMIES) {
		log_printf("hitting max number of enemies");
		return NULL;
	}

	struct Enemy *en = &gs->enemies[gs->nenemies++];
	enemy_init(en, gs->pixfmt, gs->place, fl);
	return en;
}

// runs each frame
static void add_guards_and_enemies_as_needed(struct GameState *gs)
{
	int n = 3;
	float nprob = 0.2f;    // probability to get stack of n guards instead of 1 guard

	/*
	The frequency of enemies appearing, as enemies per frame on average, is

		1/enemydelay,

	because we get one enemy in enemydelay frames. The expected value of guards
	to get (aka weighted average of guard counts with probabilities as weights) is

		nprob*n + (1 - nprob)*1,

	so the frequency of guards appearing is

		(nprob*n + (1 - nprob)*1)/guarddelay.

	If enemy and guard frequencies are equal, we have a balance of enemies and
	guards. In practice, people make mistakes, and the enemies win eventually,
	although good playing makes games last longer. This is exactly what I want for
	this game.

	Setting the frequencies equal allows me to solve guarddelay given enemydelay.
	We can still set enemydelay however we want.
	*/
	unsigned enemydelay = 5*CAMERA_FPS;
	unsigned guarddelay = (unsigned)( (nprob*n + (1 - nprob)*1)*enemydelay );

	gs->thisframe++;
	if (time_to_do_something(&gs->lastguardframe, gs->thisframe, guarddelay)) {
		int n = (rand() < (int)(nprob*(float)RAND_MAX)) ? n : 1;
		log_printf("adding a pile of %d guards", n);
		guard_create_unpickeds(gs->place, gs->pixfmt, gs->unpicked_guards, &gs->n_unpicked_guards, n);
	}
	if (time_to_do_something(&gs->lastenemyframe, gs->thisframe, enemydelay)) {
		log_printf("adding an enemy");
		add_enemy(gs, 0);
	}
}

// returns whether to continue playing
static bool handle_event(SDL_Event event, struct GameState *gs)
{
	bool down = (event.type == SDL_KEYDOWN);

	switch(event.type) {
	case SDL_QUIT:
		return false;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
		switch(event.key.keysym.scancode) {
		case SDL_SCANCODE_A: player_set_turning(&gs->players[0], -1, down); break;
		case SDL_SCANCODE_D: player_set_turning(&gs->players[0], +1, down); break;
		case SDL_SCANCODE_W: player_set_moving(&gs->players[0], down); break;
		case SDL_SCANCODE_S: player_set_flat(&gs->players[0], down); break;

		case SDL_SCANCODE_LEFT: player_set_turning(&gs->players[1], -1, down); break;
		case SDL_SCANCODE_RIGHT: player_set_turning(&gs->players[1], +1, down); break;
		case SDL_SCANCODE_UP: player_set_moving(&gs->players[1], down); break;
		case SDL_SCANCODE_DOWN: player_set_flat(&gs->players[1], down); break;

		default: break;
		}
		break;

	default: break;
	}

	return true;
}

static void handle_players_bumping_each_other(struct Player *plr1, struct Player *plr2)
{
	float bump = ellipsoid_bump_amount(&plr1->ellipsoid, &plr2->ellipsoid);
	if (bump != 0) {
		log_printf("players bump into each other");
		ellipsoid_move_apart(&plr1->ellipsoid, &plr2->ellipsoid, bump);
	}
}

static void handle_players_bumping_enemies(struct GameState *gs)
{
	for (int p = 0; p < 2; p++) {
		for (int e = gs->nenemies - 1; e >= 0; e--) {
			if (ellipsoid_bump_amount(&gs->players[p].ellipsoid, &gs->enemies[e].ellipsoid) != 0) {
				log_printf("player %d hits enemy %d", p, e);
				sound_play("farts/fart*.wav");
				int nguards = --gs->players[p].nguards;   // can become negative

				/*
				If the game is over, then don't delete the enemy. This way it
				shows up in game over screen.
				*/
				if (nguards >= 0 && !(gs->enemies[e].flags & ENEMY_NEVERDIE))
					gs->enemies[e] = gs->enemies[--gs->nenemies];
			}
		}
	}
}

static void handle_enemies_bumping_unpicked_guards(struct GameState *gs)
{
	for (int e = gs->nenemies - 1; e >= 0; e--) {
		for (int u = gs->n_unpicked_guards - 1; u >= 0; u--) {
			if (ellipsoid_bump_amount(&gs->enemies[e].ellipsoid, &gs->unpicked_guards[u]) != 0) {
				log_printf("enemy %d destroys unpicked guard %d", e, u);
				sound_play("farts/fart*.wav");
				gs->unpicked_guards[u] = gs->unpicked_guards[--gs->n_unpicked_guards];
			}
		}
	}
}

static void handle_players_bumping_unpicked_guards(struct GameState *gs)
{
	for (int p = 0; p < 2; p++) {
		for (int u = gs->n_unpicked_guards - 1; u >= 0; u--) {
			if (ellipsoid_bump_amount(&gs->players[p].ellipsoid, &gs->unpicked_guards[u]) != 0) {
				log_printf("player %d picks unpicked guard %d", p, u);
				sound_play("pick.wav");
				gs->unpicked_guards[u] = gs->unpicked_guards[--gs->n_unpicked_guards];
				gs->players[p].nguards++;
			}
		}
	}
}

static int get_all_ellipsoids(
	const struct GameState *gs, const struct Ellipsoid **arr)
{
	static struct Ellipsoid result[MAX_ELLIPSOIDS];
	static_assert(sizeof(result[0]) < 512,
		"Ellipsoid struct is huge, maybe switch to pointers?");
	struct Ellipsoid *ptr = result;

	*ptr++ = gs->players[0].ellipsoid;
	*ptr++ = gs->players[1].ellipsoid;
	ptr += guard_create_picked(ptr, &gs->players[0]);
	ptr += guard_create_picked(ptr, &gs->players[1]);

	for (int i = 0; i < gs->nenemies; i++)
		*ptr++ = gs->enemies[i].ellipsoid;

	// this will likely optimize into a memcpy, so why bother write it as memcpy :D
	for (int i = 0; i < gs->n_unpicked_guards; i++)
		*ptr++ = gs->unpicked_guards[i];

	SDL_assert(ptr < result + sizeof(result)/sizeof(result[0]));

	*arr = result;
	return ptr - result;
}

enum MiscState play_the_game(
	SDL_Window *wnd,
	const struct EllipsoidPic *plr1pic, const struct EllipsoidPic *plr2pic,
	const struct EllipsoidPic **winnerpic,
	const struct Place *pl)
{
	SDL_Surface *winsurf = SDL_GetWindowSurface(wnd);
	if (!winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	static struct GameState gs;   // static because its big struct, avoiding stack usage
	gs = (struct GameState){
		.place = pl,
		.pixfmt = winsurf->format,
		.players = {
			{
				.ellipsoid = { .angle = 0, .epic = plr1pic, .center = pl->playerlocs[0] },
				.cam = { .screencentery = winsurf->h/4, .surface = misc_create_cropped_surface(
					winsurf, (SDL_Rect){ 0, 0, winsurf->w/2, winsurf->h }) },
			},
			{
				.ellipsoid = { .angle = 0, .epic = plr2pic, .center = pl->playerlocs[1] },
				.cam = { .screencentery = winsurf->h/4, .surface = misc_create_cropped_surface(
					winsurf, (SDL_Rect){ winsurf->w/2, 0, winsurf->w/2, winsurf->h }) },
			},
		},
	};

	for (int i = 0; i < pl->nneverdielocs; i++) {
		struct Enemy *en = add_enemy(&gs, ENEMY_NEVERDIE);
		if (!en)
			continue;
		SDL_assert(en->ellipsoid.center.y == pl->neverdielocs[i].y);
		en->ellipsoid.center = pl->neverdielocs[i];
		en->flags |= ENEMY_NEVERDIE;
	}

	struct LoopTimer lt = {0};
	enum MiscState ret;

	while(gs.players[0].nguards >= 0 && gs.players[1].nguards >= 0) {
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if (!handle_event(event, &gs)) {
				ret = MISC_STATE_QUIT;
				goto out;
			}
		}

		add_guards_and_enemies_as_needed(&gs);
		for (int i = 0; i < gs.nenemies; i++)
			enemy_eachframe(&gs.enemies[i], pl);
		for (int i = 0; i < gs.n_unpicked_guards; i++)
			guard_unpicked_eachframe(&gs.unpicked_guards[i]);
		for (int i = 0; i < 2; i++)
			player_eachframe(&gs.players[i], pl);

		handle_players_bumping_each_other(&gs.players[0], &gs.players[1]);
		handle_players_bumping_enemies(&gs);
		handle_enemies_bumping_unpicked_guards(&gs);
		handle_players_bumping_unpicked_guards(&gs);

		SDL_FillRect(winsurf, NULL, 0);

		const struct Ellipsoid *els;
		int nels = get_all_ellipsoids(&gs, &els);

		for (int i = 0; i < 2; i++)
			show_all(pl->walls, pl->nwalls, els, nels, &gs.players[i].cam);

		// horizontal line
		SDL_FillRect(winsurf, &(SDL_Rect){ winsurf->w/2, 0, 1, winsurf->h }, SDL_MapRGB(winsurf->format, 0xff, 0xff, 0xff));

		SDL_UpdateWindowSurface(wnd);
		looptimer_wait(&lt);
	}
	ret = MISC_STATE_GAMEOVER;

	if (gs.players[0].nguards >= 0)
		*winnerpic = plr1pic;
	else
		*winnerpic = plr2pic;

out:
	SDL_FreeSurface(gs.players[0].cam.surface);
	SDL_FreeSurface(gs.players[1].cam.surface);
	return ret;
}
