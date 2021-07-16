#include "editplace.h"
#include "button.h"
#include "log.h"
#include "max.h"
#include "misc.h"
#include "place.h"
#include "wall.h"
#include "looptimer.h"
#include "showall.h"
#include "camera.h"
#include "gameover.h"

struct PlaceEditor {
	enum MiscState state;
	struct Place *place;
	struct Camera cam;
	int rotatedir;
	struct Wall selwall;   // if at edge of place, then entire edge selected
	struct Button deletebtn, donebtn;

	struct Wall *dndwall;  // NULL for not currently dragging
	SDL_Point dndstart;
	bool dragged;
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

static bool walls_match(const struct Wall *w1, const struct Wall *w2)
{
	return w1->dir == w2->dir && w1->startx == w2->startx && w1->startz == w2->startz;
}

static struct Wall *find_wall_from_place(struct PlaceEditor *pe, const struct Wall *w)
{
	for (int i = 0; i < pe->place->nwalls; i++) {
		if (walls_match(&pe->place->walls[i], w))
			return &pe->place->walls[i];
	}
	return NULL;
}

static bool add_wall(struct PlaceEditor *pe)
{
	SDL_assert(pe->place->nwalls <= MAX_WALLS);
	if (pe->place->nwalls == MAX_WALLS) {
		log_printf("hitting max number of walls, can't add more");
		return false;
	}

	if (find_wall_from_place(pe, &pe->selwall))
		return false;

	place_addwall(pe->place, pe->selwall.startx, pe->selwall.startz, pe->selwall.dir);
	log_printf("Added wall, now there are %d walls", pe->place->nwalls);
	place_save(pe->place);
	return true;
}

static bool is_at_edge(const struct Wall *w, const struct Place *pl)
{
	return
		(w->dir == WALL_DIR_XY && (w->startz == 0 || w->startz == pl->zsize)) ||
		(w->dir == WALL_DIR_ZY && (w->startx == 0 || w->startx == pl->xsize));
}

static void delete_wall(struct PlaceEditor *pe)
{
	struct Wall *w = find_wall_from_place(pe, &pe->selwall);
	if (w && !is_at_edge(w, pe->place)) {
		*w = pe->place->walls[--pe->place->nwalls];
		log_printf("Deleted wall, now there are %d walls", pe->place->nwalls);
		place_save(pe->place);
	}
}

static void select_wall_by_mouse(struct PlaceEditor *pe, int mousex, int mousey)
{
	// Top of place is at plane y=1. Figure out where on it we clicked
	Vec3 dir = {
		// Vector from camera towards clicked direction
		camera_screenx_to_xzr(&pe->cam, mousex),
		camera_screeny_to_yzr(&pe->cam, mousey),
		1
	};
	vec3_apply_matrix(&dir, pe->cam.cam2world);

	// cam->location + dircoeff*dir has y coordinate 1
	float dircoeff = -(pe->cam.location.y - 1)/dir.y;
	Vec3 onplane = vec3_add(pe->cam.location, vec3_mul_float(dir, dircoeff));
	if (!isfinite(onplane.y)) {
		// Happens when mouse is moved above window
		log_printf("weird y coordinate for plane: %.10f", onplane.y);
		return;
	}
	SDL_assert(fabsf(onplane.y - 1) < 1e-5f);

	if (onplane.x < -1 || onplane.x > pe->place->xsize+1 ||
		onplane.z < -1 || onplane.z > pe->place->zsize+1)
	{
		return;
	}

	switch(pe->selwall.dir) {
		// needs keep_selected_wall_within_place()
		case WALL_DIR_XY:
			pe->selwall.startz = (int)(dir.z>0 ? floorf(onplane.z) : ceilf(onplane.z)); // towards camera
			pe->selwall.startx = (int)floorf(onplane.x);
			break;
		case WALL_DIR_ZY:
			pe->selwall.startx = (int)(dir.x>0 ? floorf(onplane.x) : ceilf(onplane.x)); // towards camera
			pe->selwall.startz = (int)floorf(onplane.z);
			break;
	}
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
	// If you move selection beyond edge, it changes direction so it's parallel to the edge
	if (pe->selwall.dir == WALL_DIR_XY && (pe->selwall.startx < 0 || pe->selwall.startx >= pe->place->xsize))
		pe->selwall.dir = WALL_DIR_ZY;
	if (pe->selwall.dir == WALL_DIR_ZY && (pe->selwall.startz < 0 || pe->selwall.startz >= pe->place->zsize))
		pe->selwall.dir = WALL_DIR_XY;

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
bool handle_event(struct PlaceEditor *pe, const SDL_Event *e)
{
	button_handle_event(e, &pe->deletebtn);
	button_handle_event(e, &pe->donebtn);

	enum AngleKind ak = angle_kind(pe->cam.angle);
	switch(e->type) {
	case SDL_MOUSEBUTTONDOWN:
		pe->dndstart = (SDL_Point){ e->button.x, e->button.y };
		pe->dndwall = find_wall_from_place(pe, &pe->selwall);  // can be NULL
		if (pe->dndwall && is_at_edge(pe->dndwall, pe->place))
			pe->dndwall = NULL;
		pe->dragged = false;
		return true;

	case SDL_MOUSEMOTION:
		select_wall_by_mouse(pe, e->button.x, e->button.y);
		keep_selected_wall_within_place(pe);
		if (pe->dndwall)
			pe->dndwall->offset = (SDL_Point){
				e->button.x - pe->dndstart.x,
				e->button.y - pe->dndstart.y,
			};
		pe->dragged = true;
		return true;

	case SDL_MOUSEBUTTONUP:
		if (!pe->dragged) {
			if (!add_wall(pe))
				delete_wall(pe);
		} else if (pe->dndwall && !find_wall_from_place(pe, &pe->selwall)) {
			// Not going on top of another wall, can move
			pe->dndwall->startx = pe->selwall.startx;
			pe->dndwall->startz = pe->selwall.startz;
			place_save(pe->place);
		}

		// Restore back to non-dnd state
		if (pe->dndwall) {
			pe->dndwall->offset = (SDL_Point){0,0};
			wall_init(pe->dndwall);
			pe->dndwall = NULL;
		}
		return true;

	case SDL_KEYDOWN:
		switch(misc_handle_scancode(e->key.keysym.scancode)) {
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
			return false;
		case SDL_SCANCODE_RETURN:
			if (!add_wall(pe))
				delete_wall(pe);
			return true;
		default:
			return false;
		}

	case SDL_KEYUP:
		switch(misc_handle_scancode(e->key.keysym.scancode)) {
		case SDL_SCANCODE_A:
			if (pe->rotatedir == 1)
				pe->rotatedir = 0;
			return false;
		case SDL_SCANCODE_D:
			if (pe->rotatedir == -1)
				pe->rotatedir = 0;
			return false;
		default:
			return false;
		}

	default:
		return false;
	}
}

static bool is_selected(const struct PlaceEditor *pe, const struct Wall *w)
{
	if (is_at_edge(w, pe->place))
		return wall_linedup(&pe->selwall, w);
	return walls_match(&pe->selwall, w);
}

static void draw_walls(struct PlaceEditor *pe)
{
	// static to keep down stack usage
	static struct Wall behind[MAX_WALLS], selected[MAX_WALLS], front[MAX_WALLS];
	int nbehind=0, nselected=0, nfront=0;

	Vec3 selcenter = wall_center(&pe->selwall);
	for (const struct Wall *w = pe->place->walls; w < pe->place->walls + pe->place->nwalls; w++) {
		if (is_selected(pe, w))
			selected[nselected++] = *w;
		else if (wall_linedup(w, &pe->selwall) || wall_side(w, selcenter) == wall_side(w, pe->cam.location))
			behind[nbehind++] = *w;
		else
			front[nfront++] = *w;
	}

	show_all(behind, nbehind, false, NULL, 0, &pe->cam);
	show_all(selected, nselected, true, NULL, 0, &pe->cam);
	wall_init(&pe->selwall);
	wall_drawborder(&pe->selwall, &pe->cam);
	show_all(front, nfront, false, NULL, 0, &pe->cam);
}

static void on_done_clicked(void *data)
{
	struct PlaceEditor *pe = data;
	pe->state = MISC_STATE_CHOOSER;
}

struct DeleteData {
	struct PlaceEditor *editor;
	struct Place *places;
	int *nplaces;
};

static void delete_this_place(void *data)
{
	const struct DeleteData *dd = data;
	place_delete(dd->places, dd->nplaces, dd->editor->place - dd->places);
	dd->editor->state = MISC_STATE_CHOOSER;
}

static void set_to_true(void *ptr)
{
	bool *b = ptr;
	*b = true;
}

static void confirm_delete(struct DeleteData *dd, SDL_Window *wnd, SDL_Surface *wndsurf)
{
	log_printf("Delete button clicked, entering confirm loop");
	SDL_FillRect(wndsurf, NULL, 0);
	SDL_Surface *textsurf = misc_create_text_surface(
		"Are you sure you want to permanently delete this place?",
		(SDL_Color){0xff,0xff,0xff}, 25);

	bool noclicked = false;
	struct Button yesbtn = {
		.text = "Yes, please\ndelete it",
		.destsurf = wndsurf,
		.scancodes = { SDL_SCANCODE_Y },
		.center = { wndsurf->w/2 - button_width(0)/2, wndsurf->h/2 },
		.onclick = delete_this_place,
		.onclickdata = dd,
	};
	struct Button nobtn = {
		.text = "No, don't\ntouch it",
		.scancodes = { SDL_SCANCODE_N, SDL_SCANCODE_ESCAPE },
		.destsurf = wndsurf,
		.center = { wndsurf->w/2 + button_width(0)/2, wndsurf->h/2 },
		.onclick = set_to_true,
		.onclickdata = &noclicked,
	};

	button_show(&yesbtn);
	button_show(&nobtn);
	misc_blit_with_center(textsurf, wndsurf, &(SDL_Point){ wndsurf->w/2, wndsurf->h/4 });

	struct LoopTimer lt = {0};
	while(dd->editor->state == MISC_STATE_EDITPLACE && !noclicked) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				dd->editor->state = MISC_STATE_QUIT;
			button_handle_event(&e, &yesbtn);
			button_handle_event(&e, &nobtn);
		}

		SDL_UpdateWindowSurface(wnd);
		looptimer_wait(&lt);
	}
	SDL_FreeSurface(textsurf);
}

enum MiscState editplace_run(SDL_Window *wnd, struct Place *places, int *nplaces, int placeidx)
{
	struct Place *pl = &places[placeidx];

	SDL_Surface *wndsurf = SDL_GetWindowSurface(wnd);
	if (!wndsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());
	SDL_FillRect(wndsurf, NULL, 0);

	bool delclicked = false;
	struct PlaceEditor pe = {
		.state = MISC_STATE_EDITPLACE,
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
		.donebtn = {
			.text = "Done",
			.destsurf = wndsurf,
			.center = {
				button_width(0)/2,
				button_height(0)/2
			},
			.scancodes = { SDL_SCANCODE_ESCAPE },
			.onclick = on_done_clicked,
			.onclickdata = &pe,
		},
		.deletebtn = {
			.text = "Delete\nthis place",
			.destsurf = wndsurf,
			.center = {
				button_width(0)/2,
				button_height(0)*3/2
			},
			.onclick = set_to_true,
			.onclickdata = &delclicked,
		},
	};
	struct DeleteData dd = {
		.editor = &pe,
		.places = places,
		.nplaces = nplaces,
	};
	rotate_camera(&pe, 0);

	struct LoopTimer lt = {0};

	for (bool redraw = true; ; redraw = false) { // First iteration always redraws
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				return MISC_STATE_QUIT;

			if (handle_event(&pe, &e))
				redraw = true;
			// TODO: handle_event() should include delclicked stuff
			if (delclicked) {
				confirm_delete(&dd, wnd, wndsurf);
				delclicked = false;
				redraw = true;
			}
			if (pe.state != MISC_STATE_EDITPLACE)
				return pe.state;
		}

		if (pe.rotatedir != 0)
			redraw = true;

		if (redraw) {
			rotate_camera(&pe, pe.rotatedir * 3.0f);

			SDL_FillRect(wndsurf, NULL, 0);
			draw_walls(&pe);
			button_show(&pe.donebtn);
			button_show(&pe.deletebtn);
		}

		SDL_UpdateWindowSurface(wnd);  // Run every time, in case buttons redraw themselves
		looptimer_wait(&lt);
	}
}
