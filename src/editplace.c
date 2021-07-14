#include "editplace.h"
#include "log.h"
#include "misc.h"
#include "place.h"
#include "wall.h"
#include "looptimer.h"
#include "showall.h"
#include "camera.h"

struct PlaceEditor {
	struct Place *pl;
	struct Camera cam;
	int rotatedir;
};

static void rotate_camera(struct PlaceEditor *pe, float speed)
{
	pe->cam.angle += speed/CAMERA_FPS;

	float d = hypotf(pe->pl->xsize, pe->pl->zsize);
	Vec3 tocamera = vec3_mul_float((Vec3){ 0, 0.5f, 0.7f }, d);
	vec3_apply_matrix(&tocamera, mat3_rotation_xz(pe->cam.angle));

	Vec3 placecenter = { pe->pl->xsize/2, 0, pe->pl->zsize/2 };
	pe->cam.location = vec3_add(placecenter, tocamera);

	camera_update_caches(&pe->cam);
	SDL_FillRect(pe->cam.surface, NULL, 0);
	show_all(pe->pl->walls, pe->pl->nwalls, NULL, 0, &pe->cam);
}

void handle_event(struct PlaceEditor *pe, SDL_Event e)
{
	switch(e.type) {
	case SDL_KEYDOWN:
		switch(misc_handle_scancode(e.key.keysym.scancode)) {
			case SDL_SCANCODE_LEFT:
				pe->rotatedir = 1;
				break;
			case SDL_SCANCODE_RIGHT:
				pe->rotatedir = -1;
				break;
			default:
				log_printf("unknown key press scancode %d", e.key.keysym.scancode);
		}
		break;

	case SDL_KEYUP:
		switch(misc_handle_scancode(e.key.keysym.scancode)) {
			case SDL_SCANCODE_LEFT:
				if (pe->rotatedir == 1)
					pe->rotatedir = 0;
				break;
			case SDL_SCANCODE_RIGHT:
				if (pe->rotatedir == -1)
					pe->rotatedir = 0;
				break;
			default:
				log_printf("unknown key release scancode %d", e.key.keysym.scancode);
		}

	default:
		break;
	}
}

enum MiscState editplace_run(SDL_Window *wnd, struct Place *pl)
{
SDL_Surface *wndsurf = SDL_GetWindowSurface(wnd);
	if (!wndsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());
	SDL_FillRect(wndsurf, NULL, 0);

	struct PlaceEditor pe = {
		.pl = pl,
		.cam = {
			.screencentery = 0,
			.surface = misc_create_cropped_surface(wndsurf, (SDL_Rect){
				0, 0,
				CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT,
			}),
			.angle = 0,
		},
		.rotatedir = 0,
	};
	rotate_camera(&pe, 0);

	struct LoopTimer lt = {0};
	while(true) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				return MISC_STATE_QUIT;
			handle_event(&pe, e);
		}

		if (pe.rotatedir != 0)
			rotate_camera(&pe, pe.rotatedir * 3.0f);
		SDL_UpdateWindowSurface(wnd);
		looptimer_wait(&lt);
	}
}
