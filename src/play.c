#include "play.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "ellipsoid.h"
#include "enemy.h"
#include "guard.h"
#include "log.h"
#include "looptimer.h"
#include "map.h"
#include "max.h"
#include "misc.h"
#include "pause.h"
#include "player.h"
#include "rect3.h"
#include "showall.h"
#include "sound.h"
#include "wall.h"

// includes all the GameObjects that all players should see
struct GameState {
	const struct Map *map;
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
		// Choose random enemy location.
		// Does not take in account region size, otherwise small regions are too safe.
		pc = gs->map->enemylocs[rand() % gs->map->nenemylocs];
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
	now, but that can take a very long time.
	Also make sure that small areas containing a spawning point are not too safe.
	*/
	enemydelay = (unsigned)(enemydelay * 0.6f);
	if (gs->map->nenemylocs != 0)
		enemydelay /= gs->map->nenemylocs;

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

static enum State handle_event(SDL_Event event, struct GameState *gs, SDL_Window *wnd)
{
	bool down = (event.type == SDL_KEYDOWN);

	switch(event.type) {
	case SDL_QUIT:
		return STATE_QUIT;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
		switch(normalize_scancode(event.key.keysym.scancode)) {
			// many keyboards have numpad with zero right next to the "→" arrow, like "f" is next to "d"
			case SDL_SCANCODE_F:
				if (down) player_drop_guard(&gs->players[0], gs->unpicked_guards, &gs->n_unpicked_guards);
				break;
			case SDL_SCANCODE_0:
				if (down) player_drop_guard(&gs->players[1], gs->unpicked_guards, &gs->n_unpicked_guards);
				break;

			case SDL_SCANCODE_J:
				gs->players[0].speed.y = 100;
				break;

			case SDL_SCANCODE_A: player_set_turning(&gs->players[0], -1, down); break;
			case SDL_SCANCODE_D: player_set_turning(&gs->players[0], +1, down); break;
			case SDL_SCANCODE_W: player_set_moving(&gs->players[0], down); break;
			case SDL_SCANCODE_S: player_set_flat(&gs->players[0], down); break;

			case SDL_SCANCODE_LEFT: player_set_turning(&gs->players[1], -1, down); break;
			case SDL_SCANCODE_RIGHT: player_set_turning(&gs->players[1], +1, down); break;
			case SDL_SCANCODE_UP: player_set_moving(&gs->players[1], down); break;
			case SDL_SCANCODE_DOWN: player_set_flat(&gs->players[1], down); break;

			case SDL_SCANCODE_ESCAPE: return show_pause_screen(wnd);

			default:
				log_printf("unknown key press/release scancode %d", event.key.keysym.scancode);
		}
		return STATE_PLAY;

	default:
		return STATE_PLAY;
	}
}

static void handle_players_bumping_each_other(struct Player *plr0, struct Player *plr1)
{
	float bump = ellipsoid_bump_amount(&plr0->ellipsoid, &plr1->ellipsoid);
	if (bump != 0) {
		log_printf("players bump into each other");
		ellipsoid_move_apart(&plr0->ellipsoid, &plr1->ellipsoid, bump);
	}
}

static void handle_players_bumping_enemies(struct GameState *gs)
{
	for (int p = 0; p < 2; p++) {
		for (int e = gs->nenemies - 1; e >= 0; e--) {
			if (ellipsoid_bump_amount(&gs->players[p].ellipsoid, &gs->enemies[e].ellipsoid) != 0) {
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
			if (ellipsoid_bump_amount(&gs->enemies[e].ellipsoid, &gs->unpicked_guards[u]) != 0) {
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
			if (ellipsoid_bump_amount(&gs->players[p].ellipsoid, &gs->unpicked_guards[u]) != 0) {
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

enum State play_the_game(
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
					.center = { map->playerlocs[0].x + 0.5f, 0, map->playerlocs[0].z + 0.5f },
				},
				.cam = {
					.screencentery = winsurf->h/4,
					.surface = create_cropped_surface(
						winsurf, (SDL_Rect){ 0, 0, winsurf->w/2, winsurf->h }
					),
				},
			},
			{
				.ellipsoid = {
					.angle = 0,
					.epic = plr1pic,
					.center = { map->playerlocs[1].x + 0.5f, 0, map->playerlocs[1].z + 0.5f }
				},
				.cam = {
					.screencentery = winsurf->h/4,
					.surface = create_cropped_surface(
						winsurf, (SDL_Rect){ winsurf->w/2, 0, winsurf->w/2, winsurf->h }
					),
				},
			},
		},
	};
	for (int i = 0; i < map->nenemylocs; i++)
		add_enemy(&gs, &map->enemylocs[i]);

	static struct Rect3 wallrects[MAX_WALLS];
	for (int i = 0; i < map->nwalls; i++)
		wallrects[i] = wall_to_rect3(&map->walls[i]);

	struct LoopTimer lt = {0};
	enum State ret;

	while(gs.players[0].nguards >= 0 && gs.players[1].nguards >= 0) {
		SDL_Event e;
		while(SDL_PollEvent(&e)) {
			ret = handle_event(e, &gs, wnd);
			if (ret != STATE_PLAY)
				goto out;
		}

		add_guards_and_enemies_as_needed(&gs);
		for (int i = 0; i < gs.nenemies; i++)
			enemy_eachframe(&gs.enemies[i]);
		for (int i = 0; i < gs.n_unpicked_guards; i++)
			guard_unpicked_eachframe(&gs.unpicked_guards[i]);
		for (int i = 0; i < 2; i++)
			player_eachframe(&gs.players[i], map);

		handle_players_bumping_each_other(&gs.players[0], &gs.players[1]);
		handle_players_bumping_enemies(&gs);
		handle_enemies_bumping_unpicked_guards(&gs);
		handle_players_bumping_unpicked_guards(&gs);

		SDL_FillRect(winsurf, NULL, 0);

		const struct Ellipsoid *els;
		int nels = get_all_ellipsoids(&gs, &els);

		for (int i = 0; i < 2; i++)
			show_all(wallrects, map->nwalls, els, nels, &gs.players[i].cam);

		// horizontal line
		SDL_FillRect(winsurf, &(SDL_Rect){ winsurf->w/2, 0, 1, winsurf->h }, SDL_MapRGB(winsurf->format, 0xff, 0xff, 0xff));

		char s[100];
		sprintf(s, "%d enemies, %d unpicked guards", gs.nenemies, gs.n_unpicked_guards);
		SDL_Surface *surf = create_text_surface(s, (SDL_Color){0xff,0xff,0xff}, 20);
		SDL_BlitSurface(surf, NULL, winsurf, &(SDL_Rect){20,10});
		SDL_FreeSurface(surf);

		SDL_UpdateWindowSurface(wnd);
		looptimer_wait(&lt);
	}
	ret = STATE_GAMEOVER;

	if (gs.players[0].nguards >= 0)
		*winnerpic = plr0pic;
	else
		*winnerpic = plr1pic;

out:
	SDL_FreeSurface(gs.players[0].cam.surface);
	SDL_FreeSurface(gs.players[1].cam.surface);
	return ret;
}
