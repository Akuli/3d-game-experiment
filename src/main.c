#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "camera.h"
#include "ellipsoid.h"
#include "enemy.h"
#include "guard.h"
#include "log.h"
#include "mathstuff.h"
#include "place.h"
#include "player.h"
#include "showall.h"
#include "sound.h"
#include "wall.h"

#include <SDL2/SDL.h>

#define FPS 60

/*
unpicked guard = guard that no player has picked or enemy destroyed

Each player has at most GUARD_MAX guards, so at most 1+GUARD_MAX ellipsoids.
There are 2 players. Remaining ellipsoids are divided equally for unpicked
guards and enemies.
*/
#define MAX_ENEMIES ( (SHOWALL_MAX_ELLIPSOIDS - 2*(1 + GUARD_MAX)) / 2 )
#define MAX_UNPICKED_GUARDS MAX_ENEMIES

static_assert(MAX_UNPICKED_GUARDS + MAX_ENEMIES + 2*(1 + GUARD_MAX) <= SHOWALL_MAX_ELLIPSOIDS, "");
static_assert(MAX_UNPICKED_GUARDS >= 100, "");
static_assert(MAX_ENEMIES >= 100, "");

// includes all the GameObjects that all players should see
struct GameState {
	struct Player players[2];
	struct Enemy enemies[MAX_ENEMIES];
	int nenemies;
	const struct Place *place;

	struct Ellipsoid unpicked_guards[MAX_UNPICKED_GUARDS];
	int n_unpicked_guards;
};


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
		case SDL_SCANCODE_ESCAPE: return false;

		case SDL_SCANCODE_A: player_set_turning(&gs->players[0], -1, down); break;
		case SDL_SCANCODE_D: player_set_turning(&gs->players[0], +1, down); break;
		case SDL_SCANCODE_W: player_set_moving(&gs->players[0], down); break;
		case SDL_SCANCODE_S: player_set_flat(&gs->players[0], down); break;

		case SDL_SCANCODE_LEFT: player_set_turning(&gs->players[1], -1, down); break;
		case SDL_SCANCODE_RIGHT: player_set_turning(&gs->players[1], +1, down); break;
		case SDL_SCANCODE_UP: player_set_moving(&gs->players[1], down); break;
		case SDL_SCANCODE_DOWN: player_set_flat(&gs->players[1], down); break;

		default:
			break;
		}
		break;

	default:
		break;
	}

	return true;
}

/*
Create a surface that refers to another another surface. So, drawing to the
returned surface actually draws to the surface given as argument.
*/
static SDL_Surface *create_half_surface(SDL_Surface *surf, int xoffset, int width)
{
	SDL_Surface *res = SDL_CreateRGBSurfaceFrom(
		(uint32_t *)surf->pixels + xoffset,
		width, surf->h,
		32, surf->pitch, 0, 0, 0, 0);
	if (!res)
		log_printf_abort("SDL_CreateRGBSurfaceFrom failed: %s", SDL_GetError());
	return res;
}

static void handle_players_bumping_each_other(struct Player *plr1, struct Player *plr2)
{
	float bump = ellipsoid_bump_amount(&plr1->ellipsoid, &plr2->ellipsoid);
	if (bump != 0)
		ellipsoid_move_apart(&plr1->ellipsoid, &plr2->ellipsoid, bump);
}

static void handle_players_bumping_enemies(struct GameState *gs)
{
	for (int p = 0; p < 2; p++) {
		for (int e = gs->nenemies - 1; e >= 0; e--) {
			if (ellipsoid_bump_amount(&gs->players[p].ellipsoid, &gs->enemies[e].ellipsoid) != 0) {
				gs->enemies[e] = gs->enemies[--gs->nenemies];
				log_printf("%d enemies left", gs->nenemies);
				sound_play("farts/fart*.wav");

				int nguards = --gs->players[p].nguards;
				log_printf("player %d now has %d guards", p, nguards);
				if (nguards < 0) {
					// TODO: needs something MUCH nicer than this...
					log_printf("*********************");
					log_printf("***   game over   ***");
					log_printf("*********************");
				}
			}
		}
	}
}

static void get_all_ellipsoids(
	const struct GameState *gs, const struct Ellipsoid **arr, int *arrlen)
{
	static struct Ellipsoid result[SHOWALL_MAX_ELLIPSOIDS];
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

	assert(ptr < result + sizeof(result)/sizeof(result[0]));

	*arr = result;
	*arrlen = ptr - result;
}

int main(int argc, char **argv)
{
	srand(time(NULL));

	if (!( argc == 2 && strcmp(argv[1], "--no-sound") == 0 ))
		sound_init();

	static struct GameState gs;
	memset(&gs, 0, sizeof gs);

	SDL_Window *win = SDL_CreateWindow(
		"TODO: title here", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SHOWALL_SCREEN_WIDTH, SHOWALL_SCREEN_HEIGHT, 0);
	if (!win)
		log_printf_abort("SDL_CreateWindow failed: %s", SDL_GetError());

	SDL_Surface *winsurf = SDL_GetWindowSurface(win);
	if (!winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	gs.place = &place_list()[0];

	ellipsoidpic_load(&gs.players[0].epic, "players/Tux.png", winsurf->format);
	ellipsoidpic_load(&gs.players[1].epic, "players/Chick.png", winsurf->format);

	gs.players[0].ellipsoid.epic = &gs.players[0].epic;
	gs.players[1].ellipsoid.epic = &gs.players[1].epic;

	gs.players[0].ellipsoid.center = (Vec3){ 2.5f, 0, 0.5f };
	gs.players[1].ellipsoid.center = (Vec3){ 1.5f, 0, 0.5f };

	// This turned out to be much faster than blitting
	gs.players[0].cam.surface = create_half_surface(winsurf, 0, winsurf->w/2);
	gs.players[1].cam.surface = create_half_surface(winsurf, winsurf->w/2, winsurf->w/2);

	gs.players[0].cam.id = "cam1";
	gs.players[1].cam.id = "cam2";

	/*
	FIXME:
	- guards of only one player show up
	- enemies don't show up
	- when guard-less player hits (invisible) enemies, the other player's
	  guards go away
	*/
	gs.players[0].nguards = 20;
	gs.players[1].nguards = 20;

	gs.nenemies = 1;
	for (int i = 0; i < gs.nenemies; i++) {
		enemy_init(&gs.enemies[i], winsurf->format);
		gs.enemies[i].ellipsoid.center.x += 1;
		gs.enemies[i].ellipsoid.center.z += 2;
		ellipsoid_update_transforms(&gs.enemies[i].ellipsoid);
	}

	uint32_t time = 0;
	int counter = 0;
	float percentsum = 0;

	while(1){
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if (!handle_event(event, &gs))
				goto exit;
		}

		for (int i = 0; i < gs.nenemies; i++)
			enemy_eachframe(&gs.enemies[i], FPS, gs.place);
		for (int i = 0; i < 2; i++)
			player_eachframe(&gs.players[i], FPS, gs.place->walls, gs.place->nwalls);

		handle_players_bumping_each_other(&gs.players[0], &gs.players[1]);
		handle_players_bumping_enemies(&gs);

		SDL_FillRect(winsurf, NULL, 0);

		const struct Ellipsoid *els;
		int nels;
		get_all_ellipsoids(&gs, &els, &nels);

		for (int i = 0; i < 2; i++)
			show_all(gs.place->walls, gs.place->nwalls, els, nels, &gs.players[i].cam);

		SDL_FillRect(winsurf, &(SDL_Rect){ winsurf->w/2, 0, 1, winsurf->h }, SDL_MapRGB(winsurf->format, 0xff, 0xff, 0xff));
		SDL_UpdateWindowSurface(win);

		uint32_t curtime = SDL_GetTicks();

		float percent = (float)(curtime - time) / (1000/FPS) * 100.f;
		percentsum += percent;
		if (++counter == FPS/3) {
			log_printf("speed percentage average = %.2f%%", percentsum / (float)counter);
			counter = 0;
			percentsum = 0;
		}

		bool first = (time == 0);
		time += 1000/FPS;
		if (curtime <= time) {
			SDL_Delay(time - curtime);
		} else {
			if (!first)
				log_printf("the game is lagging with speed percentage %.2f%%", percent);
			time = curtime;
		}
	}

exit:
	SDL_FreeSurface(gs.players[0].cam.surface);
	SDL_FreeSurface(gs.players[1].cam.surface);
	sound_deinit();
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
