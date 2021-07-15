#include "editplace.h"
#include "log.h"
#include "misc.h"
#include "place.h"
#include "wall.h"
#include "looptimer.h"
#include "showall.h"
#include "camera.h"

struct PlaceEditor {
	struct Place *place;
	struct Camera cam;
	int rotatedir;
	struct Wall selwall;   // selected wall
};

static void rotate_camera(struct PlaceEditor *pe, float speed)
{
	pe->cam.angle += speed/CAMERA_FPS;

	float d = hypotf(pe->place->xsize, pe->place->zsize);
	Vec3 tocamera = vec3_mul_float((Vec3){ 0, 0.5f, 0.7f }, d);
	vec3_apply_matrix(&tocamera, mat3_rotation_xz(pe->cam.angle));

	Vec3 placecenter = { pe->place->xsize/2, 0, pe->place->zsize/2 };
	pe->cam.location = vec3_add(placecenter, tocamera);
	camera_update_caches(&pe->cam);
}

// Example: AK_XPOS = towards positive x-axis within +-45deg
enum AngleKind { AK_XPOS, AK_XNEG, AK_ZPOS, AK_ZNEG };

static enum AngleKind angle_kind(float angle)
{
	float pi = acosf(-1);
	angle = fmodf(angle, 2*pi);
	if (angle < 0)
		angle += 2*pi;

	if (0.25f*pi <= angle && angle <= 0.75f*pi) return AK_XPOS;
	if (0.75f*pi <= angle && angle  <= 1.25f*pi) return AK_ZPOS;
	if (1.25f*pi <= angle && angle  <= 1.75f*pi) return AK_XNEG;
	return AK_ZNEG;
}

static enum AngleKind rotate_90deg(enum AngleKind ak)
{
	switch(ak) {
		case AK_XPOS: return AK_ZPOS;
		case AK_ZPOS: return AK_XNEG;
		case AK_XNEG: return AK_ZNEG;
		case AK_ZNEG: return AK_XPOS;
	}
	return AK_ZPOS;   // never runs, but makes compiler happy
}

static void clamp_selwall_to_place(struct PlaceEditor *pe)
{
	int xmax = pe->place->xsize, zmax = pe->place->zsize;

	switch(pe->selwall.dir) {
		case WALL_DIR_XY: xmax--; break;
		case WALL_DIR_ZY: zmax--; break;
	}

	pe->selwall.startx = max(pe->selwall.startx, 0);
	pe->selwall.startz = max(pe->selwall.startz, 0);
	pe->selwall.startx = min(pe->selwall.startx, xmax);
	pe->selwall.startz = min(pe->selwall.startz, zmax);
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
				ak = rotate_90deg(ak);
				// fall through
			case SDL_SCANCODE_DOWN:
				ak = rotate_90deg(ak);
				// fall through
			case SDL_SCANCODE_RIGHT:
				ak = rotate_90deg(ak);
				// fall through
			case SDL_SCANCODE_UP:
				switch(ak) {
					case AK_XPOS: pe->selwall.startx++; break;
					case AK_XNEG: pe->selwall.startx--; break;
					case AK_ZPOS: pe->selwall.startz++; break;
					case AK_ZNEG: pe->selwall.startz--; break;
				}
				clamp_selwall_to_place(pe);
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
				return false;
			case SDL_SCANCODE_D:
				if (down)
					pe->rotatedir = -1;
				if (!down && pe->rotatedir == -1)
					pe->rotatedir = 0;
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
		.place = pl,
		.cam = {
			.screencentery = 0,
			.surface = misc_create_cropped_surface(wndsurf, (SDL_Rect){
				0, 0,
				CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT,
			}),
			.angle = 0,
		},
		.rotatedir = 0,
		.selwall = {
			.startx = 0,
			.startz = 0,
			.highlight = WALL_HL_BORDER_ONLY,
		},
	};
	rotate_camera(&pe, 0);

	struct LoopTimer lt = {0};
	for (bool redraw = true; ; redraw = false) { // First iteration always redraws
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
			switch (angle_kind(pe.cam.angle)) {
				case AK_XPOS:
				case AK_XNEG:
					pe.selwall.dir = WALL_DIR_ZY;
					break;
				case AK_ZPOS:
				case AK_ZNEG:
					pe.selwall.dir = WALL_DIR_XY;
					break;
			}
			clamp_selwall_to_place(&pe);

			SDL_FillRect(pe.cam.surface, NULL, 0);

			SDL_assert(pe.place->nwalls <= MAX_WALLS - 1);
			pe.place->walls[pe.place->nwalls] = pe.selwall;
			show_all(pe.place->walls, pe.place->nwalls + 1, NULL, 0, &pe.cam);

			wall_init(&pe.selwall);
			wall_drawborder(&pe.selwall, &pe.cam);

			SDL_UpdateWindowSurface(wnd);
		}
		looptimer_wait(&lt);
	}
}
