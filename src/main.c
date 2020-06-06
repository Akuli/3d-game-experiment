#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "camera.h"
#include "ellipsoid.h"
#include "enemy.h"
#include "log.h"
#include "mathstuff.h"
#include "place.h"
#include "player.h"
#include "showall.h"
#include "sound.h"
#include "wall.h"

#include <SDL2/SDL.h>

#define FPS 60

// includes all the GameObjects that all players should see
struct GameState {
	struct Player players[2];
	struct Enemy enemies[SHOWALL_MAX_ENEMIES];
	size_t nenemies;
	const struct Place *place;
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
		log_printf_abort("SDL_CreateRGBSurfaceFROM failed: %s", SDL_GetError());
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
		for (size_t e = 0; e < gs->nenemies; e++) {
			if (ellipsoid_bump_amount(&gs->players[p].ellipsoid, &gs->enemies[e].ellipsoid) == 0)
				continue;

			memmove(&gs->enemies[e], &gs->enemies[--gs->nenemies], sizeof(gs->enemies[0]));
			sound_play("farts/fart*.wav");
		}
	}
}

int main(int argc, char **argv)
{
	srand((unsigned) time(NULL));

	if (!( argc == 2 && strcmp(argv[1], "--no-sound") == 0 ))
		sound_init();

	SDL_Window *win = SDL_CreateWindow(
		"TODO: title here", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, 0);
	if (!win)
		log_printf_abort("SDL_CreateWindow failed: %s", SDL_GetError());

	SDL_Surface *winsurf = SDL_GetWindowSurface(win);
	if (!winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	struct GameState *gs = calloc(1, sizeof(*gs));
	if (!gs)
		log_printf_abort("not enough memory");

	gs->place = &place_list()[0];

	ellipsoid_load(&gs->players[0].ellipsoid, "players/Tux.png");
	ellipsoid_load(&gs->players[1].ellipsoid, "players/Chick.png");

	gs->players[0].ellipsoid.center = (Vec3){ 2.5f, 0, 0.5f };
	gs->players[1].ellipsoid.center = (Vec3){ 1.5f, 0, 0.5f };

	// This turned out to be much faster than blitting
	gs->players[0].cam.surface = create_half_surface(winsurf, 0, winsurf->w/2);
	gs->players[1].cam.surface = create_half_surface(winsurf, winsurf->w/2, winsurf->w/2);

	gs->nenemies = 10;
	for (size_t i = 0; i < gs->nenemies; i++) {
		enemy_init(&gs->enemies[i]);
		gs->enemies[i].ellipsoid.center.x += 1;
		gs->enemies[i].ellipsoid.center.z += 1;
		gs->enemies[i].ellipsoid.angle += i/acosf(-1);
		ellipsoid_update_transforms(&gs->enemies[i].ellipsoid);
	}

	uint32_t time = 0;
	int counter = 0;
	float percentsum = 0;

	while(1){
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if (!handle_event(event, gs))
				goto exit;
		}

		for (size_t i = 0; i < gs->nenemies; i++)
			enemy_eachframe(&gs->enemies[i], FPS, gs->place);
		for (int i=0; i < 2; i++)
			player_eachframe(&gs->players[i], FPS, gs->place->walls, gs->place->nwalls);

		handle_players_bumping_each_other(&gs->players[0], &gs->players[1]);
		handle_players_bumping_enemies(gs);

		SDL_FillRect(winsurf, NULL, 0);

		for (int i = 0; i < 2; i++)
			show_all(gs->place->walls, gs->place->nwalls, gs->players, 2, gs->enemies, gs->nenemies, &gs->players[i].cam);

		SDL_FillRect(winsurf, &(SDL_Rect){ winsurf->w/2, 0, 1, winsurf->h }, SDL_MapRGB(winsurf->format, 0xff, 0xff, 0xff));
		SDL_UpdateWindowSurface(win);

		uint32_t curtime = SDL_GetTicks();

		percentsum += (float)(curtime - time) / (1000/FPS) * 100.f;
		if (++counter == FPS) {
			fprintf(stderr, "speed percentage average = %.2f%%\n", percentsum / (float)counter);
			counter = 0;
			percentsum = 0;
		}

		time += 1000/FPS;
		if (curtime <= time) {
			SDL_Delay(time - curtime);
		} else {
			// its lagging
			time = curtime;
		}
	}

exit:
	SDL_FreeSurface(gs->players[0].cam.surface);
	SDL_FreeSurface(gs->players[1].cam.surface);
	free(gs);
	sound_deinit();
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
