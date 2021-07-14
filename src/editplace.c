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
	int selx, selz;  // selected rectangle
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
	show_all(pe->pl->walls, pe->pl->nwalls, NULL, 0, &pe->cam, NULL);

	struct Wall hlwall = { pe->selx, pe->selz, WALL_DIR_XY }; // FIXME dir
	wall_init(&hlwall);

	struct WallCache wc;
	int dummy1, dummy2;
	if (wall_visible_xminmax_fillcache(&hlwall, &pe->cam, &dummy1, &dummy2, &wc))
		wall_drawborder(&wc);
}

// Example: AK_XPOS = towards positive x-axis within +-45deg
enum AngleKind { AK_XPOS, AK_XNEG, AK_ZPOS, AK_ZNEG };

static enum AngleKind angle_kind(float angle)
{
	float pi = acosf(-1);
	angle = fmodf(angle, 2*pi);
	if (angle < 0)
		angle += 2*pi;

	log_printf("angle = %f", angle);

	if (0.25f*pi <= angle && angle <= 0.75f*pi) return AK_XPOS;
	if (0.75f*pi <= angle && angle  <= 1.25f*pi) return AK_ZPOS;
	if (1.25f*pi <= angle && angle  <= 1.75f*pi) return AK_XNEG;
	return AK_ZNEG;
}

static enum AngleKind rotate_90deg_plus(enum AngleKind ak)
{
	if (ak == AK_XPOS)
		return AK_ZPOS;
	if (ak == AK_ZPOS)
		return AK_XNEG;
	if (ak == AK_XNEG)
		return AK_ZNEG;
	if (ak == AK_ZNEG)
		return AK_XPOS;
	// TODO: switch case
}

static enum AngleKind rotate_90deg_minus(enum AngleKind ak)
{
	ak = rotate_90deg_plus(ak);
	ak = rotate_90deg_plus(ak);
	ak = rotate_90deg_plus(ak);
	return ak;
}

// Returns whether redrawing needed
bool handle_event(struct PlaceEditor *pe, SDL_Event e)
{
	bool down = false;
	enum AngleKind ak = angle_kind(pe->cam.angle);

	switch(e.type) {

	case SDL_KEYDOWN:
		down = true;

		switch(misc_handle_scancode(e.key.keysym.scancode)) {
			case SDL_SCANCODE_LEFT:
				ak = rotate_90deg_plus(ak);
				// fall through
			case SDL_SCANCODE_DOWN:
				ak = rotate_90deg_plus(ak);
				// fall through
			case SDL_SCANCODE_RIGHT:
				ak = rotate_90deg_plus(ak);
				// fall through
			case SDL_SCANCODE_UP:
				switch(ak) {
					case AK_XPOS: pe->selx++; break;
					case AK_XNEG: pe->selx--; break;
					case AK_ZPOS: pe->selz++; break;
					case AK_ZNEG: pe->selz--; break;
				}
				return true;
			default:
				break;
		}

		// fall through
	case SDL_KEYUP:
		switch(misc_handle_scancode(e.key.keysym.scancode)) {
			case SDL_SCANCODE_A:
				if (down)
					pe->rotatedir = 1;
				if (!down && pe->rotatedir == 1)
					pe->rotatedir = 0;
				log_printf("AK = %d", angle_kind(pe->cam.angle));
				return false;
			case SDL_SCANCODE_D:
				if (down)
					pe->rotatedir = -1;
				if (!down && pe->rotatedir == -1)
					pe->rotatedir = 0;
				log_printf("AK = %d", angle_kind(pe->cam.angle));
				return false;
		}
		log_printf("unknown key press/release scancode %d", e.key.keysym.scancode);
		break;

	default:
		break;
	}

	return false;
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
	bool redraw = true;
	while(true) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				return MISC_STATE_QUIT;
			if (handle_event(&pe, e))
				redraw = true;;
		}

		if (pe.rotatedir != 0)
			redraw = true;

		if (redraw) {
			rotate_camera(&pe, pe.rotatedir * 3.0f);
			SDL_UpdateWindowSurface(wnd);
		}
		looptimer_wait(&lt);

		redraw = false;   // Not-first iterations don't always redraw
	}
}
