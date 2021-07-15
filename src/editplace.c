#include "editplace.h"
#include "log.h"
#include "max.h"
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

static void keep_selected_wall_within_place(struct PlaceEditor *pe)
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
	enum AngleKind ak = angle_kind(pe->cam.angle);

	switch(e.type) {
	case SDL_KEYDOWN:
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
			keep_selected_wall_within_place(pe);
			return true;
		case SDL_SCANCODE_A:
			pe->rotatedir = 1;
			return false;
		case SDL_SCANCODE_D:
			pe->rotatedir = -1;
			return false;  // go to keyup case
		default:
			log_printf("unknown key press scancode %d", e.key.keysym.scancode);
			return false;
		}

	case SDL_KEYUP:
		switch(misc_handle_scancode(e.key.keysym.scancode)) {
		case SDL_SCANCODE_LEFT:
		case SDL_SCANCODE_DOWN:
		case SDL_SCANCODE_RIGHT:
		case SDL_SCANCODE_UP:
			return false;
		case SDL_SCANCODE_A:
			if (pe->rotatedir == 1)
				pe->rotatedir = 0;
			return false;
		case SDL_SCANCODE_D:
			if (pe->rotatedir == -1)
				pe->rotatedir = 0;
			return false;
		default:
			log_printf("unknown key release scancode %d", e.key.keysym.scancode);
			return false;
		}

	default:
		return false;
	}
}

static void draw_walls(struct PlaceEditor *pe)
{
	int idx = -1;
	for (int i = 0; i < pe->place->nwalls; i++) {
		if (idx == -1 &&
			pe->place->walls[i].dir == pe->selwall.dir &&
			pe->place->walls[i].startx == pe->selwall.startx &&
			pe->place->walls[i].startz == pe->selwall.startz)
		{
			idx = i;
			break;
		}
	}

	// static to keep down stack usage
	static struct Wall behind[MAX_WALLS], front[MAX_WALLS];
	int nbehind = 0, nfront = 0;

	int hlidx = -1;
	Vec3 hlcenter = wall_center(&pe->selwall);
	for (const struct Wall *w = pe->place->walls; w < pe->place->walls + pe->place->nwalls; w++) {
		if (wall_linedup(w, &pe->selwall) || wall_side(w, hlcenter) == wall_side(w, pe->cam.location)) {
			behind[nbehind++] = *w;
			if (w->dir == pe->selwall.dir &&
				w->startx == pe->selwall.startx &&
				w->startz == pe->selwall.startz)
			{
				hlidx = nbehind-1;
			}
		} else {
			front[nfront++] = *w;
		}
	}

	show_all(behind, nbehind, behind + hlidx, NULL, 0, &pe->cam);
	wall_init(&pe->selwall);
	wall_drawborder(&pe->selwall, &pe->cam);
	show_all(front, nfront, NULL, NULL, 0, &pe->cam);
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
			keep_selected_wall_within_place(&pe);

			SDL_FillRect(pe.cam.surface, NULL, 0);
			/*
			int nwalls = highlight_selected_wall(&pe);
			show_all(pe.place->walls, nwalls, NULL, 0, &pe.cam);
			unhighlight_walls(pe.place);
			*/
			draw_walls(&pe);

			SDL_UpdateWindowSurface(wnd);
		}
		looptimer_wait(&lt);
	}
}
