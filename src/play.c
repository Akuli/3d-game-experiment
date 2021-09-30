#include "play.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "ellipsoid.h"
#include "enemy.h"
#include "guard.h"
#include "intersect.h"
#include "log.h"
#include "looptimer.h"
#include "max.h"
#include "misc.h"
#include "map.h"
#include "player.h"
#include "showall.h"
#include "sound.h"
#include "region.h"

// includes all the GameObjects that all players should see
struct GameState {
	const struct Map *map;
	const SDL_PixelFormat *pixfmt;

	struct Player players[2];

	struct Enemy enemies[MAX_ENEMIES];
	int nenemies;

	// How many squares can reach from enemy location. Same length as map->enemylocs
	int enemyregionsizes[MAX_ENEMIES];

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

static void add_enemy(struct GameState *gs, const struct MapCoords *coordptr)
{
	if (gs->map->nenemylocs == 0) {  // avoid crash in "% 0" below
		log_printf("map has no enemies");
		return;
	}

	SDL_assert(gs->nenemies <= MAX_ENEMIES);
	if (gs->nenemies == MAX_ENEMIES) {
		log_printf("hitting MAX_ENEMIES=%d", MAX_ENEMIES);
		return;
	}

	struct MapCoords pc;
	if (coordptr)
		pc = *coordptr;
	else {
		// Choose random enemy location. Use region sizes as weights.
		int sum = 0;
		for (int i = 0; i < gs->map->nenemylocs; i++)
			sum += gs->enemyregionsizes[i];
		int val = rand() % sum;
		int lo = 0;
		int i = 0;
		while(1) {
			SDL_assert(i < gs->map->nenemylocs);
			int hi = lo + gs->enemyregionsizes[i];
			if (lo <= val && val < hi)
				break;
			lo = hi;
			i++;
		}
		pc = gs->map->enemylocs[i];
	}

	gs->enemies[gs->nenemies++] = enemy_new(gs->map, pc);
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
	guards. Setting the frequencies equal allows me to solve guarddelay given enemydelay.
	We can still set enemydelay however we want.
	*/
	unsigned enemydelay = 5*CAMERA_FPS;
	unsigned guarddelay = (unsigned)( (nprob*n + (1 - nprob)*1)*enemydelay );

	/*
	People make mistakes, and the enemies win eventually, even with the variables we have
	now, but that can take a very long time. Making this slightly off makes games shorter.
	*/
	enemydelay = (unsigned)(enemydelay * 0.8f);

	gs->thisframe++;
	if (time_to_do_something(&gs->lastguardframe, gs->thisframe, guarddelay)) {
		int toadd = (rand() < (int)(nprob*(float)RAND_MAX)) ? n : 1;
		log_printf("There are %d unpicked guards, adding %d more", gs->n_unpicked_guards, toadd);
		guard_create_unpickeds_random(gs->unpicked_guards, &gs->n_unpicked_guards, toadd, gs->map);
	}
	if (time_to_do_something(&gs->lastenemyframe, gs->thisframe, enemydelay)) {
		log_printf("There are %d enemies, adding one more", gs->nenemies);
		add_enemy(gs, NULL);
	}
}

static void handle_event(SDL_Event event, struct GameState *gs)
{
	bool down = (event.type == SDL_KEYDOWN);

	switch(event.type) {
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		switch(misc_handle_scancode(event.key.keysym.scancode)) {
			// many keyboards have numpad with zero right next to the "→" arrow, like "f" is next to "d"
			case SDL_SCANCODE_F:
				if (down) player_drop_guard(&gs->players[0], gs->unpicked_guards, &gs->n_unpicked_guards);
				break;
			case SDL_SCANCODE_0:
				if (down) player_drop_guard(&gs->players[1], gs->unpicked_guards, &gs->n_unpicked_guards);
				break;

			case SDL_SCANCODE_A: player_set_turning(&gs->players[0], -1, down); break;
			case SDL_SCANCODE_D: player_set_turning(&gs->players[0], +1, down); break;
			case SDL_SCANCODE_W: player_set_moving(&gs->players[0], down); break;
			case SDL_SCANCODE_S: player_set_flat(&gs->players[0], down); break;

			case SDL_SCANCODE_LEFT: player_set_turning(&gs->players[1], -1, down); break;
			case SDL_SCANCODE_RIGHT: player_set_turning(&gs->players[1], +1, down); break;
			case SDL_SCANCODE_UP: player_set_moving(&gs->players[1], down); break;
			case SDL_SCANCODE_DOWN: player_set_flat(&gs->players[1], down); break;

			default:
				log_printf("unknown key press/release scancode %d", event.key.keysym.scancode);
		}
		break;

	default:
		break;
	}
}

static void handle_players_bumping_enemies(struct GameState *gs)
{
	for (int p = 0; p < 2; p++) {
		for (int e = gs->nenemies - 1; e >= 0; e--) {
			if (intersects_el_el(&gs->players[p].ellipsoid, &gs->enemies[e].ellipsoid)) {
				log_printf(
					"enemy %d/%d hits player %d (%d guards)",
					e, gs->nenemies,
					p, gs->players[p].nguards);
				sound_play("farts/fart*.wav");
				int nguards = --gs->players[p].nguards;   // can become negative

				/*
				If the game is over, then don't delete the enemy. This way it
				shows up in game over screen.
				*/
				if (nguards >= 0)
					gs->enemies[e] = gs->enemies[--gs->nenemies];
			}
		}
	}
}

static void handle_enemies_bumping_unpicked_guards(struct GameState *gs)
{
	for (int e = gs->nenemies - 1; e >= 0; e--) {
		for (int u = gs->n_unpicked_guards - 1; u >= 0; u--) {
			if (intersects_el_el(&gs->enemies[e].ellipsoid, &gs->unpicked_guards[u])) {
				log_printf("enemy %d/%d destroys unpicked guard %d/%d",
					e, gs->nenemies, u, gs->n_unpicked_guards);
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
			if (intersects_el_el(&gs->players[p].ellipsoid, &gs->unpicked_guards[u])) {
				log_printf(
					"player %d (%d guards) picks unpicked guard %d/%d",
					p, gs->players[p].nguards, u, gs->n_unpicked_guards);
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

static void handle_players_bumping_each_other(struct Ellipsoid *el1, struct Ellipsoid *el2)
{
	Vec3 mv;
	intersect_el_el(el1, el2, &mv);
	if (mv.y == 0) {
		// jumping into another player with not enough height moves both players
		vec3_add_inplace(&el1->botcenter, vec3_mul_float(mv, 0.5f));
		vec3_add_inplace(&el2->botcenter, vec3_mul_float(mv, -0.5f));
		return;
	}

	// move the ellipsoid above so it hopefully stays on top
	if (el1->botcenter.y > el2->botcenter.y)
		vec3_add_inplace(&el1->botcenter, mv);
	else
		vec3_sub_inplace(&el2->botcenter, mv);
}

enum MiscState play_the_game(
	SDL_Window *wnd,
	const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic,
	const struct EllipsoidPic **winnerpic,
	const struct Map *map)
{
	SDL_Surface *winsurf = SDL_GetWindowSurface(wnd);
	if (!winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	static struct GameState gs;   // static because its big struct, avoiding stack usage
	gs = (struct GameState){
		.nenemies = 0,
		.map = map,
		.pixfmt = winsurf->format,
		.players = {
			{
				.ellipsoid = {
					.angle = 0,
					.epic = plr0pic,
					.botcenter = { map->playerlocs[0].x + 0.5f, 0, map->playerlocs[0].z + 0.5f },
					.botradius = PLAYER_BOTRADIUS,
					.height = PLAYER_HEIGHT_NOFLAT,
				},
				.cam = {
					.screencentery = winsurf->h/4,
					.surface = misc_create_cropped_surface(
						winsurf, (SDL_Rect){ 0, 0, winsurf->w/2, winsurf->h }
					),
				},
			},
			{
				.ellipsoid = {
					.angle = 0,
					.epic = plr1pic,
					.botcenter = { map->playerlocs[1].x + 0.5f, 0, map->playerlocs[1].z + 0.5f },
					.botradius = PLAYER_BOTRADIUS,
					.height = PLAYER_HEIGHT_NOFLAT,
				},
				.cam = {
					.screencentery = winsurf->h/4,
					.surface = misc_create_cropped_surface(
						winsurf, (SDL_Rect){ winsurf->w/2, 0, winsurf->w/2, winsurf->h }
					),
				},
			},
		},
	};
	for (int i = 0; i < map->nenemylocs; i++) {
		gs.enemyregionsizes[i] = region_size(map, map->enemylocs[i]);
		add_enemy(&gs, &map->enemylocs[i]);
	}

	struct LoopTimer lt = {0};
	enum MiscState ret;

	while(gs.players[0].nguards >= 0 && gs.players[1].nguards >= 0) {
		SDL_Event e;
		while(SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				ret = MISC_STATE_QUIT;
				goto out;
			}
			handle_event(e, &gs);
		}

		add_guards_and_enemies_as_needed(&gs);
		for (int i = 0; i < gs.nenemies; i++)
			enemy_eachframe(&gs.enemies[i]);
		for (int i = 0; i < gs.n_unpicked_guards; i++)
			guard_unpicked_eachframe(&gs.unpicked_guards[i]);
		player_eachframe(&gs.players[0], map);
		player_eachframe(&gs.players[1], map);

		handle_players_bumping_each_other(&gs.players[0].ellipsoid, &gs.players[1].ellipsoid);

		handle_players_bumping_enemies(&gs);
		handle_enemies_bumping_unpicked_guards(&gs);
		handle_players_bumping_unpicked_guards(&gs);

		SDL_FillRect(winsurf, NULL, 0);

		const struct Ellipsoid *els;
		int nels = get_all_ellipsoids(&gs, &els);

		for (int i = 0; i < 2; i++)
			show_all(map->walls, map->nwalls, false, els, nels, &gs.players[i].cam);

		// horizontal line
		SDL_FillRect(winsurf, &(SDL_Rect){ winsurf->w/2, 0, 1, winsurf->h }, SDL_MapRGB(winsurf->format, 0xff, 0xff, 0xff));

		SDL_UpdateWindowSurface(wnd);
		looptimer_wait(&lt);
	}
	ret = MISC_STATE_GAMEOVER;

	if (gs.players[0].nguards >= 0)
		*winnerpic = plr0pic;
	else
		*winnerpic = plr1pic;

out:
	SDL_FreeSurface(gs.players[0].cam.surface);
	SDL_FreeSurface(gs.players[1].cam.surface);
	return ret;
}
