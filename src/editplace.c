#include "editplace.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include "mathstuff.h"
#include "enemy.h"
#include "button.h"
#include "log.h"
#include "max.h"
#include "misc.h"
#include "place.h"
#include "player.h"
#include "wall.h"
#include "looptimer.h"
#include "showall.h"
#include "camera.h"

struct EllipsoidEdit {
	struct Ellipsoid el;
	struct PlaceCoords *loc;
};

enum SelectMode { SEL_NONE, SEL_RESIZE, SEL_WALL, SEL_MVWALL, SEL_ELLIPSOID, SEL_MVELLIPSOID };
struct ResizeData {
	struct Wall *walls[MAX_PLACE_SIZE];
	int nwalls;
	struct Wall mainwall;  // This is the wall whose border is highlighted during resize
	bool negative;   // true if shrinks/expands in negative x or z direction
};
struct Selection {
	enum SelectMode mode;
	union {
		struct EllipsoidEdit *eledit;  // SEL_ELLIPSOID and SEL_MVELLIPSOID
		struct Wall wall;              // SEL_WALL
		struct Wall *mvwall;           // SEL_MVWALL
		struct ResizeData resize;      // SEL_RESIZE
	} data;
};

struct PlaceEditor {
	enum MiscState state;
	struct Place *place;
	struct EllipsoidEdit eledits[3];
	int neledits;
	struct Camera cam;
	int rotatedir;
	struct Button deletebtn, donebtn;
	struct Selection sel;
	bool mousemoved;
	bool up, down, left, right;   // are arrow keys pressed
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

static struct Wall *find_wall_from_place(const struct Wall *w, struct Place *pl)
{
	for (int i = 0; i < pl->nwalls; i++) {
		if (wall_match(&pl->walls[i], w))
			return &pl->walls[i];
	}
	return NULL;
}

static bool add_wall(struct PlaceEditor *pe)
{
	SDL_assert(pe->sel.mode == SEL_WALL);
	SDL_assert(pe->place->nwalls <= MAX_WALLS);
	if (pe->place->nwalls == MAX_WALLS) {
		log_printf("hitting max number of walls, can't add more");
		return false;
	}

	if (find_wall_from_place(&pe->sel.data.wall, pe->place))
		return false;

	place_addwall(pe->place, pe->sel.data.wall.startx, pe->sel.data.wall.startz, pe->sel.data.wall.dir);
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

static void delete_wall(struct PlaceEditor *pe, struct Wall *w)
{
	w = find_wall_from_place(w, pe->place);
	if (w && !is_at_edge(w, pe->place)) {
		*w = pe->place->walls[--pe->place->nwalls];
		log_printf("Deleted wall, now there are %d walls", pe->place->nwalls);
		place_save(pe->place);
	}
}

static bool wall_is_within_place(const struct Wall *w, const struct Place *pl)
{
	int xmax = pl->xsize;
	int zmax = pl->zsize;
	switch(w->dir) {
		case WALL_DIR_XY: xmax--; break;
		case WALL_DIR_ZY: zmax--; break;
	}
	return 0 <= w->startx && w->startx <= xmax && 0 <= w->startz && w->startz <= zmax;
}

static void keep_wall_within_place(const struct PlaceEditor *pe, struct Wall *w, bool resize)
{
	int xmin = 0, xmax = pe->place->xsize;
	int zmin = 0, zmax = pe->place->zsize;
	if (resize) {
		switch(w->dir) {
			case WALL_DIR_XY:
				if (pe->sel.data.resize.negative) {
					zmin = pe->place->zsize - MAX_PLACE_SIZE;
					zmax = pe->place->zsize - 2;
				} else {
					zmin = 2;
					zmax = MAX_PLACE_SIZE;
				}
				break;
			case WALL_DIR_ZY:
				if (pe->sel.data.resize.negative) {
					xmin = pe->place->xsize - MAX_PLACE_SIZE;
					xmax = pe->place->xsize - 2;
				} else {
					xmin = 2;
					xmax = MAX_PLACE_SIZE;
				}
				break;
		}
	}

	switch(w->dir) {
		case WALL_DIR_XY: xmax--; break;
		case WALL_DIR_ZY: zmax--; break;
	}

	w->startx = max(w->startx, xmin);
	w->startx = min(w->startx, xmax);
	w->startz = max(w->startz, zmin);
	w->startz = min(w->startz, zmax);
	wall_init(w);
}

static bool mouse_is_on_ellipsoid(const struct Camera *cam, const struct Ellipsoid *el, int x, int y)
{
	int xmin, xmax;
	if (!ellipsoid_visible_xminmax(el, cam, &xmin, &xmax) || !(xmin <= x && x <= xmax))
		return false;

	int ymin, ymax;
	struct EllipsoidXCache exc;
	ellipsoid_yminmax(el, cam, x, &exc, &ymin, &ymax);
	return ymin <= y && y <= ymax;
}

static bool mouse_is_on_wall(const struct Camera *cam, const struct Wall *w, int x, int y)
{
	struct WallCache wc;
	int xmin, xmax;
	if (!wall_visible_xminmax_fillcache(w, cam, &xmin, &xmax, &wc) || !(xmin <= x && x <= xmax))
		return false;

	int ymin, ymax;
	wall_yminmax(&wc, x, &ymin, &ymax);
	return ymin <= y && y <= ymax;
}

static bool mouse_is_on_ellipsoid_with_no_walls_between(struct PlaceEditor *pe, const struct Ellipsoid *el, int x, int y)
{
	if (!mouse_is_on_ellipsoid(&pe->cam, el, x, y))
		return false;

	for (const struct Wall *w = pe->place->walls; w < &pe->place->walls[pe->place->nwalls]; w++) {
		if (wall_side(w, pe->cam.location) != wall_side(w, el->center) && mouse_is_on_wall(&pe->cam, w, x, y))
			return false;
	}
	return true;
}

static bool project_mouse_to_top_of_place(
	const struct PlaceEditor *pe, int mousex, int mousey, float *x, float *z)
{
	// Top of place is at plane y=1. Figure out where on it we clicked
	Vec3 cam2clickdir = {
		// Vector from camera towards clicked direction
		camera_screenx_to_xzr(&pe->cam, mousex),
		camera_screeny_to_yzr(&pe->cam, mousey),
		1
	};
	vec3_apply_matrix(&cam2clickdir, pe->cam.cam2world);

	// p.y should be 1, i.e. top of place
	float dircoeff = -(pe->cam.location.y - 1)/cam2clickdir.y;
	Vec3 p = vec3_add(pe->cam.location, vec3_mul_float(cam2clickdir, dircoeff));
	if (!isfinite(p.x) || !isfinite(p.z))  // e.g. mouse moved to top of screen
		return false;

	*x = p.x;
	*z = p.z;
	return true;
}

static bool find_wall_by_mouse_location(const struct PlaceEditor *pe, struct Wall *dst, int mousex, int mousey)
{
	float fx, fz;
	if (!project_mouse_to_top_of_place(pe, mousex, mousey, &fx, &fz))
		return false;

	int x = (int)floorf(fx);
	int z = (int)floorf(fz);
	struct Wall couldbe[] = {
		{ .startx = x,   .startz = z,   .dir = WALL_DIR_XY },
		{ .startx = x,   .startz = z,   .dir = WALL_DIR_ZY },
		{ .startx = x,   .startz = z+1, .dir = WALL_DIR_XY },
		{ .startx = x+1, .startz = z,   .dir = WALL_DIR_ZY },
	};

	for (int i = 0; i < 4; i++) {
		wall_init(&couldbe[i]);
		if (mouse_is_on_wall(&pe->cam, &couldbe[i], mousex, mousey)) {
			*dst = couldbe[i];
			return true;
		}
	}
	return false;
}

static void do_resize(struct PlaceEditor *pe, int mousex, int mousey)
{
	SDL_assert(pe->sel.mode == SEL_RESIZE);

	float x, z;
	if (!project_mouse_to_top_of_place(pe, mousex, mousey, &x, &z))
		return;

	pe->sel.data.resize.mainwall.startx = (int)roundf(x);
	pe->sel.data.resize.mainwall.startz = (int)roundf(z);

	keep_wall_within_place(pe, &pe->sel.data.resize.mainwall, true);
	wall_init(&pe->sel.data.resize.mainwall);
	for (int i = 0; i < pe->sel.data.resize.nwalls; i++) {
		switch(pe->sel.data.resize.mainwall.dir) {
		case WALL_DIR_XY:
			pe->sel.data.resize.walls[i]->startz = pe->sel.data.resize.mainwall.startz;
			break;
		case WALL_DIR_ZY:
			pe->sel.data.resize.walls[i]->startx = pe->sel.data.resize.mainwall.startx;
			break;
		}
		wall_init(pe->sel.data.resize.walls[i]);
	}
}

static void move_wall(struct PlaceEditor *pe, int mousex, int mousey)
{
	SDL_assert(pe->sel.mode == SEL_MVWALL);

	struct Wall w;
	if (find_wall_by_mouse_location(pe, &w, mousex, mousey)) {
		keep_wall_within_place(pe, &w, false);
		if (!find_wall_from_place(&w, pe->place)) {
			// Not going on top of another wall, can move
			*pe->sel.data.mvwall = w;
			wall_init(pe->sel.data.mvwall);
			place_save(pe->place);
		}
	}
}

static bool place_contains_something_at(const struct Place *pl, int x, int z)
{
	if (pl->enemyloc.x == x && pl->enemyloc.z == z) return true;
	for (int p=0; p<2; p++) {
		if (pl->playerlocs[p].x == x && pl->playerlocs[p].z == z)
			return true;
	}
	for (int i=0; i < pl->nneverdielocs; i++) {
		if (pl->neverdielocs[i].x == x && pl->neverdielocs[i].z == z)
			return true;
	}
	return false;
}

static void move_ellipsoid(const struct PlaceEditor *pe, int mousex, int mousey, struct PlaceCoords *loc)
{
	float xf, zf;
	project_mouse_to_top_of_place(pe, mousex, mousey, &xf, &zf);
	int x = (int)floorf(xf);
	int z = (int)floorf(zf);

	if (x < 0) x = 0;
	if (x >= pe->place->xsize) x = pe->place->xsize-1;
	if (z < 0) z = 0;
	if (z >= pe->place->zsize) z = pe->place->zsize-1;

	if (!place_contains_something_at(pe->place, x, z)) {
		loc->x = x;
		loc->z = z;
		place_save(pe->place);
	}
}

static void select_by_mouse_coords(struct PlaceEditor *pe, int mousex, int mousey)
{
	// Find ellipsoid visible with no walls between having smallest distance to camera
	int idx = -1;
	float smallestd = HUGE_VALF;
	for (int i = 0; i < pe->neledits; i++) {
		if (mouse_is_on_ellipsoid_with_no_walls_between(pe, &pe->eledits[i].el, mousex, mousey)) {
			float d = vec3_lengthSQUARED(vec3_sub(pe->eledits[i].el.center, pe->cam.location));
			if (d < smallestd) {
				idx = i;
				smallestd = d;
			}
		}
	}

	if (idx == -1) {
		// No ellipsoids under mouse
		struct Wall w;
		if (find_wall_by_mouse_location(pe, &w, mousex, mousey) && wall_is_within_place(&w, pe->place))
			pe->sel = (struct Selection){ .mode = SEL_WALL, .data = { .wall = w }};
		else
			pe->sel = (struct Selection){ .mode = SEL_NONE };
	} else {
		pe->sel = (struct Selection){
			.mode = SEL_ELLIPSOID,
			.data = { .eledit = &pe->eledits[idx] },
		};
	}
}

static void on_arrow_key(struct PlaceEditor *pe, float angle, bool oppositespressed)
{
	switch(pe->sel.mode) {
		case SEL_NONE:
		case SEL_WALL:
		case SEL_ELLIPSOID:
			break;

		case SEL_MVELLIPSOID:
		case SEL_MVWALL:
		case SEL_RESIZE:
			return;  // TODO: more keyboard functionality
	}

	float pi = acosf(-1);
	angle = fmodf(angle, 2*pi);
	if (angle < 0)
		angle += 2*pi;

	// Trial and error has been used to figure out what to do in each case
	int dx = 0, dz = 0;
	if (0.25f*pi <= angle && angle <= 0.75f*pi)
		dx = -1;
	else if (0.75f*pi <= angle && angle <= 1.25f*pi)
		dz = -1;
	else if (1.25f*pi <= angle && angle <= 1.75f*pi)
		dx = 1;
	else
		dz = 1;

	switch(pe->sel.mode) {
	case SEL_NONE:
		pe->sel = (struct Selection){ .mode = SEL_WALL, .data = { .wall = {0}}};
		// fall through
	case SEL_WALL:
		if (oppositespressed) {
			pe->sel.data.wall.dir = dz ? WALL_DIR_ZY : WALL_DIR_XY;
			keep_wall_within_place(pe, &pe->sel.data.wall, false);
		}

		// Check if we are going towards an ellipsoid
		if ((pe->sel.data.wall.dir == WALL_DIR_ZY && dx) ||
			(pe->sel.data.wall.dir == WALL_DIR_XY && dz))
		{
			int px = pe->sel.data.wall.startx + min(0, dx);
			int pz = pe->sel.data.wall.startz + min(0, dz);
			for (int i = 0; i < pe->neledits; i++) {
				if (pe->eledits[i].loc->x == px && pe->eledits[i].loc->z == pz) {
					pe->sel = (struct Selection){
						.mode = SEL_ELLIPSOID,
						.data = { .eledit = &pe->eledits[i] },
					};
					return;
				}
			}
		}

		pe->sel.data.wall.startx += dx;
		pe->sel.data.wall.startz += dz;
		keep_wall_within_place(pe, &pe->sel.data.wall, false);
		break;

	case SEL_ELLIPSOID:
		// Select wall near ellipsoid
		{
			const struct EllipsoidEdit *ee = pe->sel.data.eledit;
			pe->sel = (struct Selection) {
				.mode = SEL_WALL,
				.data = {
					.wall = {
						.dir = (dx ? WALL_DIR_ZY : WALL_DIR_XY),
						.startx = ee->loc->x + max(0, dx),
						.startz = ee->loc->z + max(0, dz),
					}
				}
			};
		}
		break;

	default:
		// TODO: more key bindings
		break;
	}
}

static struct ResizeData begin_resize(const struct Wall *edgewall, struct Place *pl)
{
	log_printf("Resize begins");
	struct ResizeData rd = { .mainwall = *edgewall, .nwalls = 0 };
	switch(edgewall->dir) {
		case WALL_DIR_XY: rd.negative = (edgewall->startz == 0); break;
		case WALL_DIR_ZY: rd.negative = (edgewall->startx == 0); break;
	}

	for (struct Wall *w = pl->walls; w < &pl->walls[pl->nwalls]; w++) {
		if (wall_linedup(w, edgewall))
			rd.walls[rd.nwalls++] = w;
	}
	SDL_assert(rd.nwalls == pl->xsize || rd.nwalls == pl->zsize);
	return rd;
}

static void finish_resize(struct PlaceEditor *pe)
{
	SDL_assert(pe->sel.mode == SEL_RESIZE);
	if (pe->sel.data.resize.negative) {
		switch(pe->sel.data.resize.mainwall.dir) {
			case WALL_DIR_XY:
				place_movecontent(pe->place, 0, -pe->sel.data.resize.mainwall.startz);
				pe->place->zsize -= pe->sel.data.resize.mainwall.startz;
				break;
			case WALL_DIR_ZY:
				place_movecontent(pe->place, -pe->sel.data.resize.mainwall.startx, 0);
				pe->place->xsize -= pe->sel.data.resize.mainwall.startx;
				break;
		}
	} else {
		switch(pe->sel.data.resize.mainwall.dir) {
			case WALL_DIR_XY:
				pe->place->zsize = pe->sel.data.resize.mainwall.startz;
				break;
			case WALL_DIR_ZY:
				pe->place->xsize = pe->sel.data.resize.mainwall.startx;
				break;
		}
	}

	place_fix(pe->place);
	place_save(pe->place);
}

// Returns whether redrawing needed
bool handle_event(struct PlaceEditor *pe, const SDL_Event *e)
{
	float pi = acosf(-1);

	button_handle_event(e, &pe->deletebtn);
	button_handle_event(e, &pe->donebtn);

	// If "Yes, delete this place" button was clicked and the place no longer
	// exists, we must avoid handling the click event again
	if (pe->state != MISC_STATE_EDITPLACE)
		return false;

	switch(e->type) {
	case SDL_MOUSEBUTTONDOWN:
		switch (pe->sel.mode) {
		case SEL_WALL:
			if (is_at_edge(&pe->sel.data.wall, pe->place)) {
				struct Wall w = pe->sel.data.wall;
				pe->sel = (struct Selection) {
					.mode = SEL_RESIZE,
					.data = { .resize = begin_resize(&w, pe->place) },
				};
			} else {
				struct Wall *w = find_wall_from_place(&pe->sel.data.wall, pe->place);
				if(w)
					pe->sel = (struct Selection) { .mode = SEL_MVWALL, .data = { .mvwall = w }};
			}
			break;

		case SEL_ELLIPSOID:
			pe->sel.mode = SEL_MVELLIPSOID;
			break;

		default:
			break;
		}
		pe->mousemoved = false;
		return true;

	case SDL_MOUSEBUTTONUP:
		switch(pe->sel.mode) {
		case SEL_RESIZE:
			log_printf("Resize ends");
			finish_resize(pe);
			break;
		case SEL_MVWALL:
			log_printf("Moving a wall ends, mousemoved = %d", pe->mousemoved);
			if (!pe->mousemoved)
				delete_wall(pe, pe->sel.data.mvwall);
			break;
		case SEL_WALL:
			log_printf("Click ends");
			add_wall(pe);
			break;
		default:
			break;
		}
		pe->sel.mode = SEL_NONE;
		// fall through

	case SDL_MOUSEMOTION:
		switch(pe->sel.mode) {
		case SEL_MVELLIPSOID:
			move_ellipsoid(pe, e->button.x, e->button.y, pe->sel.data.eledit->loc);
			break;
		case SEL_MVWALL:
			move_wall(pe, e->button.x, e->button.y);
			break;
		case SEL_RESIZE:
			do_resize(pe, e->button.x, e->button.y);
			break;
		default:
			select_by_mouse_coords(pe, e->button.x, e->button.y);
			break;
		}
		pe->mousemoved = true;
		return true;

	case SDL_KEYDOWN:
		switch(misc_handle_scancode(e->key.keysym.scancode)) {
		case SDL_SCANCODE_DOWN:
			pe->down = true;
			on_arrow_key(pe, pe->cam.angle, pe->up && pe->down);
			return true;
		case SDL_SCANCODE_LEFT:
			pe->left = true;
			on_arrow_key(pe, pe->cam.angle + pi/2, pe->left && pe->right);
			return true;
		case SDL_SCANCODE_UP:
			pe->up = true;
			on_arrow_key(pe, pe->cam.angle + pi, pe->up && pe->down);
			return true;
		case SDL_SCANCODE_RIGHT:
			pe->right = true;
			on_arrow_key(pe, pe->cam.angle + 3*pi/2, pe->left && pe->right);
			return true;
		case SDL_SCANCODE_A:
			pe->rotatedir = 1;
			return false;
		case SDL_SCANCODE_D:
			pe->rotatedir = -1;
			return false;
		case SDL_SCANCODE_RETURN:
			if (pe->sel.mode == SEL_WALL) {
				if (!add_wall(pe))
					delete_wall(pe, &pe->sel.data.wall);
			}
			return true;
		default:
			return false;
		}

	case SDL_KEYUP:
		switch(misc_handle_scancode(e->key.keysym.scancode)) {
			case SDL_SCANCODE_UP: pe->up = false; return false;
			case SDL_SCANCODE_DOWN: pe->down = false; return false;
			case SDL_SCANCODE_LEFT: pe->left = false; return false;
			case SDL_SCANCODE_RIGHT: pe->right = false; return false;

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

static bool wall_should_be_highlighted(const struct PlaceEditor *pe, const struct Wall *w)
{
	switch(pe->sel.mode) {
		case SEL_MVWALL: return wall_match(pe->sel.data.mvwall, w);
		case SEL_RESIZE: return wall_linedup(&pe->sel.data.resize.mainwall, w);
		default: return false;
	}
}

struct ToShow {
	struct Wall walls[MAX_WALLS];
	int nwalls;
	struct Ellipsoid els[3];
	int nels;
};

static void show_editor(struct PlaceEditor *pe)
{
	// static to keep down stack usage
	static struct ToShow behind, select, front;
	behind.nwalls = behind.nels = 0;
	select.nwalls = select.nels = 0;
	front.nwalls = front.nels = 0;

	for (int i = 0; i < pe->neledits; i++) {
		pe->eledits[i].el.highlighted =
			(pe->sel.mode == SEL_ELLIPSOID || pe->sel.mode == SEL_MVELLIPSOID)
			&& pe->sel.data.eledit == &pe->eledits[i];
	}

	struct Wall *hlwall;
	switch(pe->sel.mode) {
	case SEL_MVWALL:
		hlwall = pe->sel.data.mvwall;
		break;
	case SEL_RESIZE:
		hlwall = &pe->sel.data.resize.mainwall;
		break;
	case SEL_WALL:
		hlwall = &pe->sel.data.wall;
		break;
	default:
		hlwall = NULL;
	}

	if (hlwall) {
		for (const struct Wall *w = pe->place->walls; w < pe->place->walls + pe->place->nwalls; w++) {
			if (wall_should_be_highlighted(pe, w))
				select.walls[select.nwalls++] = *w;
			else if (hlwall && wall_side(hlwall, wall_center(w)) == wall_side(hlwall, pe->cam.location) && !wall_linedup(hlwall, w))
				front.walls[front.nwalls++] = *w;
			else
				behind.walls[behind.nwalls++] = *w;
		}

		for (int i = 0; i < pe->neledits; i++) {
			if (wall_side(hlwall, pe->eledits[i].el.center) == wall_side(hlwall, pe->cam.location))
				front.els[front.nels++] = pe->eledits[i].el;
			else
				behind.els[behind.nels++] = pe->eledits[i].el;
		}
	} else {
		// Everything is "behind", could also use front
		behind.nwalls = pe->place->nwalls;
		for (int i=0; i < behind.nwalls; i++)
			behind.walls[i] = pe->place->walls[i];

		behind.nels = pe->neledits;
		for (int i = 0; i < pe->neledits; i++)
			behind.els[i] = pe->eledits[i].el;
	}

	show_all(behind.walls, behind.nwalls, false, behind.els, behind.nels, &pe->cam);
	show_all(select.walls, select.nwalls, true,  select.els, select.nels, &pe->cam);
	if (hlwall) {
		wall_init(hlwall);
		wall_drawborder(hlwall, &pe->cam);
	}
	show_all(front.walls,  front.nwalls,  false, front.els,  front.nels,  &pe->cam);
}

static void on_done_clicked(void *data)
{
	struct PlaceEditor *pe = data;
	pe->state = MISC_STATE_CHOOSER;
}

struct DeleteData {
	SDL_Window *wnd;
	SDL_Surface *wndsurf;
	struct PlaceEditor *editor;
	struct Place *places;
	int *nplaces;
};

static void set_to_true(void *ptr)
{
	*(bool *)ptr = true;
}

static void confirm_delete(void *ptr)
{
	log_printf("Delete button clicked, entering confirm loop");

	struct DeleteData *dd = ptr;
	SDL_FillRect(dd->wndsurf, NULL, 0);
	SDL_Surface *textsurf = misc_create_text_surface(
		"Are you sure you want to permanently delete this place?",
		(SDL_Color){0xff,0xff,0xff}, 25);

	bool yesclicked = false;
	bool noclicked = false;
	struct Button yesbtn = {
		.text = "Yes, please\ndelete it",
		.destsurf = dd->wndsurf,
		.scancodes = { SDL_SCANCODE_Y },
		.center = { dd->wndsurf->w/2 - button_width(0)/2, dd->wndsurf->h/2 },
		.onclick = set_to_true,
		.onclickdata = &yesclicked,
	};
	struct Button nobtn = {
		.text = "No, don't\ntouch it",
		.scancodes = { SDL_SCANCODE_N, SDL_SCANCODE_ESCAPE },
		.destsurf = dd->wndsurf,
		.center = { dd->wndsurf->w/2 + button_width(0)/2, dd->wndsurf->h/2 },
		.onclick = set_to_true,
		.onclickdata = &noclicked,
	};

	button_show(&yesbtn);
	button_show(&nobtn);
	misc_blit_with_center(textsurf, dd->wndsurf, &(SDL_Point){ dd->wndsurf->w/2, dd->wndsurf->h/4 });

	struct LoopTimer lt = {0};
	while(!yesclicked && !noclicked) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				dd->editor->state = MISC_STATE_QUIT;
				goto out;
			}
			button_handle_event(&e, &yesbtn);
			button_handle_event(&e, &nobtn);
		}
		SDL_UpdateWindowSurface(dd->wnd);
		looptimer_wait(&lt);
	}

	if (yesclicked) {
		place_delete(dd->places, dd->nplaces, dd->editor->place - dd->places);
		dd->editor->state = MISC_STATE_CHOOSER;
	}

out:
	SDL_FreeSurface(textsurf);
}

enum MiscState editplace_run(
	SDL_Window *wnd,
	struct Place *places, int *nplaces, int placeidx,
	const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic)
{
	struct Place *pl = &places[placeidx];

	SDL_Surface *wndsurf = SDL_GetWindowSurface(wnd);
	if (!wndsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());
	SDL_FillRect(wndsurf, NULL, 0);

	struct DeleteData deldata = {
		.wnd = wnd,
		.wndsurf = wndsurf,
		.places = places,
		.nplaces = nplaces,
	};
	struct PlaceEditor pe = {
		.sel = { .mode = SEL_NONE },
		.state = MISC_STATE_EDITPLACE,
		.place = pl,
		.eledits = {
			{
				.el = {
					.xzradius = PLAYER_XZRADIUS,
					.yradius = PLAYER_YRADIUS_NOFLAT,
					.epic = plr0pic,
					.center = { .y = PLAYER_YRADIUS_NOFLAT },
				},
				.loc = &pl->playerlocs[0],
			},
			{
				// TODO: rename variables to plr0pic and plr0pic
				.el = {
					.xzradius = PLAYER_XZRADIUS,
					.yradius = PLAYER_YRADIUS_NOFLAT,
					.epic = plr1pic,
					.center = { .y = PLAYER_YRADIUS_NOFLAT },
				},
				.loc = &pl->playerlocs[1],
			},
			{
				.el = {
					.xzradius = ENEMY_XZRADIUS,
					.yradius = ENEMY_YRADIUS,
					.epic = enemy_getfirstepic()
				},
				.loc = &pl->enemyloc,
			},
		},
		.neledits = 3,
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
			.onclick = confirm_delete,
			.onclickdata = &deldata,
		},
	};

	deldata.editor = &pe;
	for (int i = 0; i < pe.neledits; i++)
		ellipsoid_update_transforms(&pe.eledits[i].el);

	struct LoopTimer lt = {0};

	for (bool redraw = true; ; redraw = false) { // First iteration always redraws
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				return MISC_STATE_QUIT;

			if (handle_event(&pe, &e))
				redraw = true;
			if (pe.state != MISC_STATE_EDITPLACE)
				return pe.state;
		}

		if (pe.rotatedir != 0)
			redraw = true;

		if (redraw) {
			for (int i = 0; i < pe.neledits; i++) {
				pe.eledits[i].el.center.x = pe.eledits[i].loc->x + 0.5f;
				pe.eledits[i].el.center.z = pe.eledits[i].loc->z + 0.5f;
				ellipsoid_update_transforms(&pe.eledits[i].el);
			}
			rotate_camera(&pe, pe.rotatedir * 3.0f);

			SDL_FillRect(wndsurf, NULL, 0);
			show_editor(&pe);
			button_show(&pe.donebtn);
			button_show(&pe.deletebtn);
		}

		SDL_UpdateWindowSurface(wnd);  // Run every time, in case buttons redraw themselves
		looptimer_wait(&lt);
	}
}
