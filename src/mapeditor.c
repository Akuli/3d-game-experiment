#include "mapeditor.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include "mathstuff.h"
#include "enemy.h"
#include "button.h"
#include "log.h"
#include "max.h"
#include "misc.h"
#include "map.h"
#include "player.h"
#include "wall.h"
#include "looptimer.h"
#include "showall.h"
#include "camera.h"

struct EllipsoidEdit {
	struct Ellipsoid el;
	struct MapCoords *loc;
};

enum SelectMode { SEL_NONE, SEL_RESIZE, SEL_WALL, SEL_MVWALL, SEL_ELLIPSOID, SEL_MVELLIPSOID };
struct ResizeData {
	struct Wall *walls[MAX_MAPSIZE];
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

struct MapEditor {
	SDL_Window *wnd;
	enum MiscState state;
	struct Map *map;
	struct EllipsoidEdit playeredits[2];
	struct EllipsoidEdit enemyedits[MAX_ENEMIES];
	struct Camera cam;
	int rotatedir;
	struct Button delmapbtn, donebtn;
	struct Button addenemybtn;
	struct Selection sel;
	bool up, down, left, right;   // are arrow keys pressed
	bool redraw;

	// for deleting
	struct Map *maps;
	int *nmaps;
};

static bool next_ellipsoid_edit(struct MapEditor *ed, struct EllipsoidEdit **ptr)
{
	if (*ptr == NULL)
		*ptr = &ed->playeredits[0];
	else
		++*ptr;

	// Make sure this works when there are no enemy locations
	if (*ptr == &ed->playeredits[2])
		*ptr = &ed->enemyedits[0];
	if (*ptr == &ed->enemyedits[ed->map->nenemylocs])
		*ptr = NULL;

	return !!*ptr;
}

// Hard to make it const-safe, but let's hide it in a function like any other ugly thing
static bool next_ellipsoid_edit_const(const struct MapEditor *ed, const struct EllipsoidEdit **ptr)
{
	return next_ellipsoid_edit((void*)ed, (void*)ptr);
}

static void rotate_camera(struct MapEditor *ed, float speed)
{
	ed->cam.angle += speed/CAMERA_FPS;

	float d = hypotf(ed->map->xsize, ed->map->zsize);
	clamp_float(&d, 8, HUGE_VALF);

	// d is inversely proportional to surface size, camera further away when surface is small
	d *= (float)CAMERA_SCREEN_WIDTH / ed->cam.surface->w;

	Vec3 tocamera = vec3_mul_float((Vec3){ 0, 0.5f, 0.7f }, d);
	vec3_apply_matrix(&tocamera, mat3_rotation_xz(ed->cam.angle));

	Vec3 mapcenter = { ed->map->xsize*0.5f, 0, ed->map->zsize*0.5f };
	ed->cam.location = vec3_add(mapcenter, tocamera);
	camera_update_caches(&ed->cam);
}

static struct Wall *find_wall_from_map(const struct Wall *w, struct Map *map)
{
	for (int i = 0; i < map->nwalls; i++) {
		if (wall_match(&map->walls[i], w))
			return &map->walls[i];
	}
	return NULL;
}

static void add_wall(struct MapEditor *ed)
{
	SDL_assert(ed->sel.mode == SEL_WALL);
	SDL_assert(ed->map->nwalls <= MAX_WALLS);
	if (ed->map->nwalls == MAX_WALLS) {
		log_printf("hitting max number of walls, can't add more");
		return;
	}

	if (!find_wall_from_map(&ed->sel.data.wall, ed->map)) {
		map_addwall(ed->map, ed->sel.data.wall.startx, ed->sel.data.wall.startz, ed->sel.data.wall.dir);
		log_printf("Added wall, now there are %d walls", ed->map->nwalls);
		map_save(ed->map);
	}
}

static bool is_at_edge(const struct Wall *w, const struct Map *map)
{
	return
		(w->dir == WALL_DIR_XY && (w->startz == 0 || w->startz == map->zsize)) ||
		(w->dir == WALL_DIR_ZY && (w->startx == 0 || w->startx == map->xsize));
}

static bool wall_is_within_map(const struct Wall *w, const struct Map *map)
{
	int xmax = map->xsize;
	int zmax = map->zsize;
	switch(w->dir) {
		case WALL_DIR_XY: xmax--; break;
		case WALL_DIR_ZY: zmax--; break;
	}
	return 0 <= w->startx && w->startx <= xmax && 0 <= w->startz && w->startz <= zmax;
}

static void keep_wall_within_map(const struct MapEditor *ed, struct Wall *w, bool resize)
{
	int xmin = 0, xmax = ed->map->xsize;
	int zmin = 0, zmax = ed->map->zsize;
	if (resize) {
		switch(w->dir) {
			case WALL_DIR_XY:
				if (ed->sel.data.resize.negative) {
					zmin = ed->map->zsize - MAX_MAPSIZE;
					zmax = ed->map->zsize - 2;
				} else {
					zmin = 2;
					zmax = MAX_MAPSIZE;
				}
				break;
			case WALL_DIR_ZY:
				if (ed->sel.data.resize.negative) {
					xmin = ed->map->xsize - MAX_MAPSIZE;
					xmax = ed->map->xsize - 2;
				} else {
					xmin = 2;
					xmax = MAX_MAPSIZE;
				}
				break;
		}
	}

	switch(w->dir) {
		case WALL_DIR_XY: xmax--; break;
		case WALL_DIR_ZY: zmax--; break;
	}

	clamp(&w->startx, xmin, xmax);
	clamp(&w->startz, zmin, zmax);
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

static bool mouse_is_on_ellipsoid_with_no_walls_between(struct MapEditor *ed, const struct Ellipsoid *el, int x, int y)
{
	if (!mouse_is_on_ellipsoid(&ed->cam, el, x, y))
		return false;

	for (const struct Wall *w = ed->map->walls; w < &ed->map->walls[ed->map->nwalls]; w++) {
		if (wall_side(w, ed->cam.location) != wall_side(w, el->center) && mouse_is_on_wall(&ed->cam, w, x, y))
			return false;
	}
	return true;
}

static bool project_mouse_to_top_of_map(
	const struct MapEditor *ed, int mousex, int mousey, float *x, float *z)
{
	// Top of map is at plane y=1. Figure out where on it we clicked
	Vec3 cam2clickdir = {
		// Vector from camera towards clicked direction
		camera_screenx_to_xzr(&ed->cam, mousex),
		camera_screeny_to_yzr(&ed->cam, mousey),
		1
	};
	vec3_apply_matrix(&cam2clickdir, ed->cam.cam2world);

	// p.y should be 1, i.e. top of map
	float dircoeff = -(ed->cam.location.y - 1)/cam2clickdir.y;
	Vec3 p = vec3_add(ed->cam.location, vec3_mul_float(cam2clickdir, dircoeff));
	if (!isfinite(p.x) || !isfinite(p.z))  // e.g. mouse moved to top of screen
		return false;

	*x = p.x;
	*z = p.z;
	return true;
}

static bool mouse_location_to_wall(const struct MapEditor *ed, struct Wall *dst, int mousex, int mousey)
{
	float fx, fz;
	if (!project_mouse_to_top_of_map(ed, mousex, mousey, &fx, &fz))
		return false;

	int x = (int)floorf(fx);
	int z = (int)floorf(fz);
	struct Wall couldbe[] = {
		{ .startx = x,   .startz = z,   .dir = WALL_DIR_XY },
		{ .startx = x,   .startz = z,   .dir = WALL_DIR_ZY },
		{ .startx = x,   .startz = z+1, .dir = WALL_DIR_XY },
		{ .startx = x+1, .startz = z,   .dir = WALL_DIR_ZY },
	};

	for (int i = 0; i < sizeof(couldbe)/sizeof(couldbe[0]); i++) {
		wall_init(&couldbe[i]);
		if (mouse_is_on_wall(&ed->cam, &couldbe[i], mousex, mousey)) {
			*dst = couldbe[i];
			return true;
		}
	}
	return false;
}

static void do_resize(struct MapEditor *ed, int mousex, int mousey)
{
	SDL_assert(ed->sel.mode == SEL_RESIZE);

	float x, z;
	if (!project_mouse_to_top_of_map(ed, mousex, mousey, &x, &z))
		return;

	ed->sel.data.resize.mainwall.startx = (int)roundf(x);
	ed->sel.data.resize.mainwall.startz = (int)roundf(z);

	keep_wall_within_map(ed, &ed->sel.data.resize.mainwall, true);
	wall_init(&ed->sel.data.resize.mainwall);
	for (struct Wall *const *w = ed->sel.data.resize.walls; w < &ed->sel.data.resize.walls[ed->sel.data.resize.nwalls]; w++)
	{
		switch(ed->sel.data.resize.mainwall.dir) {
		case WALL_DIR_XY:
			(*w)->startz = ed->sel.data.resize.mainwall.startz;
			break;
		case WALL_DIR_ZY:
			(*w)->startx = ed->sel.data.resize.mainwall.startx;
			break;
		}
		wall_init(*w);
	}
}

static void move_wall(struct MapEditor *ed, int mousex, int mousey)
{
	SDL_assert(ed->sel.mode == SEL_MVWALL);

	struct Wall w;
	if (mouse_location_to_wall(ed, &w, mousex, mousey)) {
		keep_wall_within_map(ed, &w, false);
		if (!find_wall_from_map(&w, ed->map)) {
			// Not going on top of another wall, can move
			*ed->sel.data.mvwall = w;
			wall_init(ed->sel.data.mvwall);
			map_save(ed->map);
		}
	}
}

static bool find_ellipsoid_by_coords(const struct MapEditor *ed, int x, int z)
{
	for (const struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit_const(ed, &ee); ) {
		if (ee->loc->x == x && ee->loc->z == z)
			return true;
	}
	return false;
}

static void move_ellipsoid(const struct MapEditor *ed, int mousex, int mousey, struct MapCoords *loc)
{
	float xf, zf;
	project_mouse_to_top_of_map(ed, mousex, mousey, &xf, &zf);
	int x = (int)floorf(xf);
	int z = (int)floorf(zf);

	clamp(&x, 0, ed->map->xsize-1);
	clamp(&z, 0, ed->map->zsize-1);

	if (!find_ellipsoid_by_coords(ed, x, z)) {
		loc->x = x;
		loc->z = z;
		map_save(ed->map);
	}
}

static void select_by_mouse_coords(struct MapEditor *ed, int mousex, int mousey)
{
	float smallestd = HUGE_VALF;
	struct EllipsoidEdit *nearest = NULL;

	// Find ellipsoid visible with no walls between having smallest distance to camera
	for (struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit(ed, &ee); ) {
		if (mouse_is_on_ellipsoid_with_no_walls_between(ed, &ee->el, mousex, mousey)) {
			float d = vec3_lengthSQUARED(vec3_sub(ee->el.center, ed->cam.location));
			if (d < smallestd) {
				nearest = ee;
				smallestd = d;
			}
		}
	}

	if (nearest) {
		ed->sel = (struct Selection){
			.mode = SEL_ELLIPSOID,
			.data = { .eledit = nearest },
		};
	} else {
		// No ellipsoids under mouse
		struct Wall w;
		if (mouse_location_to_wall(ed, &w, mousex, mousey) && wall_is_within_map(&w, ed->map))
			ed->sel = (struct Selection){ .mode = SEL_WALL, .data = { .wall = w }};
		else
			ed->sel = (struct Selection){ .mode = SEL_NONE };
	}
}

static void on_arrow_key(struct MapEditor *ed, float angle, bool oppositespressed)
{
	switch(ed->sel.mode) {
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

	switch(ed->sel.mode) {
	case SEL_NONE:
		ed->sel = (struct Selection){ .mode = SEL_WALL, .data = { .wall = {0}}};
		// fall through
	case SEL_WALL:
		if (oppositespressed) {
			ed->sel.data.wall.dir = dz ? WALL_DIR_ZY : WALL_DIR_XY;
			keep_wall_within_map(ed, &ed->sel.data.wall, false);
		}

		// Check if we are going towards an ellipsoid
		if ((ed->sel.data.wall.dir == WALL_DIR_ZY && dx) ||
			(ed->sel.data.wall.dir == WALL_DIR_XY && dz))
		{
			int px = ed->sel.data.wall.startx + min(0, dx);
			int pz = ed->sel.data.wall.startz + min(0, dz);
			for (struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit(ed, &ee); ) {
				if (ee->loc->x == px && ee->loc->z == pz) {
					ed->sel = (struct Selection){
						.mode = SEL_ELLIPSOID,
						.data = { .eledit = ee },
					};
					return;
				}
			}
		}

		ed->sel.data.wall.startx += dx;
		ed->sel.data.wall.startz += dz;
		keep_wall_within_map(ed, &ed->sel.data.wall, false);
		break;

	case SEL_ELLIPSOID:
		// Select wall near ellipsoid
		{
			const struct EllipsoidEdit *ee = ed->sel.data.eledit;
			ed->sel = (struct Selection) {
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

static void delete_selected(struct MapEditor *ed)
{
	log_printf("Trying to delete selected item");
	switch(ed->sel.mode) {
		case SEL_ELLIPSOID:
		{
			struct EllipsoidEdit *ee = ed->sel.data.eledit;
			// Watch out for ub, comparing pointers to different arrays is undefined
			if (ee != &ed->playeredits[0] && ee != &ed->playeredits[1]) {
				SDL_assert(&ed->enemyedits[0] <= ee && ee < &ed->enemyedits[ed->map->nenemylocs]);

				// Once ellipsoid location is deleted, must select something else
				struct Wall w = { .startx = ee->loc->x, .startz = ee->loc->z };

				*ee->loc = ed->map->enemylocs[--ed->map->nenemylocs];
				log_printf("Deleted enemy, now there are %d enemies", ed->map->nenemylocs);
				ed->sel = (struct Selection){ .mode = SEL_WALL, .data = { .wall = w }};
				map_save(ed->map);
			}
			break;
		}

		case SEL_WALL:
		{
			struct Wall *w = find_wall_from_map(&ed->sel.data.wall, ed->map);
			if (w && !is_at_edge(w, ed->map)) {
				*w = ed->map->walls[--ed->map->nwalls];
				log_printf("Deleted wall, now there are %d walls", ed->map->nwalls);
				map_save(ed->map);
			}
			break;
		}

		default:
			break;
	}
}

static struct ResizeData begin_resize(const struct Wall *edgewall, struct Map *map)
{
	struct ResizeData rd = { .mainwall = *edgewall, .nwalls = 0 };
	switch(edgewall->dir) {
		case WALL_DIR_XY: rd.negative = (edgewall->startz == 0); break;
		case WALL_DIR_ZY: rd.negative = (edgewall->startx == 0); break;
	}

	for (struct Wall *w = map->walls; w < &map->walls[map->nwalls]; w++) {
		if (wall_linedup(w, edgewall))
			rd.walls[rd.nwalls++] = w;
	}
	SDL_assert(rd.nwalls == map->xsize || rd.nwalls == map->zsize);
	return rd;
}

static void finish_resize(struct MapEditor *ed)
{
	SDL_assert(ed->sel.mode == SEL_RESIZE);
	if (ed->sel.data.resize.negative) {
		switch(ed->sel.data.resize.mainwall.dir) {
			case WALL_DIR_XY:
				map_movecontent(ed->map, 0, -ed->sel.data.resize.mainwall.startz);
				ed->map->zsize -= ed->sel.data.resize.mainwall.startz;
				break;
			case WALL_DIR_ZY:
				map_movecontent(ed->map, -ed->sel.data.resize.mainwall.startx, 0);
				ed->map->xsize -= ed->sel.data.resize.mainwall.startx;
				break;
		}
	} else {
		switch(ed->sel.data.resize.mainwall.dir) {
			case WALL_DIR_XY:
				ed->map->zsize = ed->sel.data.resize.mainwall.startz;
				break;
			case WALL_DIR_ZY:
				ed->map->xsize = ed->sel.data.resize.mainwall.startx;
				break;
		}
	}

	map_fix(ed->map);
	map_save(ed->map);
}

#define LEFT_CLICK 1
#define RIGHT_CLICK 3

// Returns whether redrawing needed
static bool handle_event(struct MapEditor *ed, const SDL_Event *e)
{
	float pi = acosf(-1);

	button_handle_event(e, &ed->delmapbtn);
	button_handle_event(e, &ed->donebtn);
	button_handle_event(e, &ed->addenemybtn);

	// If "Yes, delete this map" button was clicked and the map no longer
	// exists, we must avoid handling the click event again
	if (ed->state != MISC_STATE_MAPEDITOR)
		return false;

	switch(e->type) {
	case SDL_MOUSEBUTTONDOWN:
		switch(e->button.button) {
		case RIGHT_CLICK:
			delete_selected(ed);
			return true;

		case LEFT_CLICK:
			switch (ed->sel.mode) {
			case SEL_WALL:
				if (is_at_edge(&ed->sel.data.wall, ed->map)) {
					log_printf("Resize begins");
					struct Wall w = ed->sel.data.wall;
					ed->sel = (struct Selection) {
						.mode = SEL_RESIZE,
						.data = { .resize = begin_resize(&w, ed->map) },
					};
				} else {
					struct Wall *w = find_wall_from_map(&ed->sel.data.wall, ed->map);
					if(w) {
						log_printf("Moving wall begins");
						ed->sel = (struct Selection) { .mode = SEL_MVWALL, .data = { .mvwall = w }};
					}
				}
				break;

			case SEL_ELLIPSOID:
				ed->sel.mode = SEL_MVELLIPSOID;
				break;

			default:
				break;
			}
			return true;

		default:
			return false;
		}

	case SDL_MOUSEBUTTONUP:
		if (e->button.button == LEFT_CLICK) {
			switch(ed->sel.mode) {
			case SEL_RESIZE:
				log_printf("Resize ends");
				finish_resize(ed);
				break;
			case SEL_MVWALL:
				log_printf("Moving a wall ends");
				break;
			case SEL_WALL:
				log_printf("Clicked some place with no wall in it, adding wall");
				add_wall(ed);
				break;
			default:
				break;
			}
			ed->sel.mode = SEL_NONE;
		}
		// fall through

	case SDL_MOUSEMOTION:
		switch(ed->sel.mode) {
		case SEL_MVELLIPSOID:
			move_ellipsoid(ed, e->button.x, e->button.y, ed->sel.data.eledit->loc);
			break;
		case SEL_MVWALL:
			move_wall(ed, e->button.x, e->button.y);
			break;
		case SEL_RESIZE:
			do_resize(ed, e->button.x, e->button.y);
			break;
		default:
			select_by_mouse_coords(ed, e->button.x, e->button.y);
			break;
		}
		return true;

	case SDL_KEYDOWN:
		switch(misc_handle_scancode(e->key.keysym.scancode)) {
		case SDL_SCANCODE_DOWN:
			ed->down = true;
			on_arrow_key(ed, ed->cam.angle, ed->up && ed->down);
			return true;
		case SDL_SCANCODE_LEFT:
			ed->left = true;
			on_arrow_key(ed, ed->cam.angle + pi/2, ed->left && ed->right);
			return true;
		case SDL_SCANCODE_UP:
			ed->up = true;
			on_arrow_key(ed, ed->cam.angle + pi, ed->up && ed->down);
			return true;
		case SDL_SCANCODE_RIGHT:
			ed->right = true;
			on_arrow_key(ed, ed->cam.angle + 3*pi/2, ed->left && ed->right);
			return true;
		case SDL_SCANCODE_A:
			ed->rotatedir = 1;
			return false;
		case SDL_SCANCODE_D:
			ed->rotatedir = -1;
			return false;
		case SDL_SCANCODE_RETURN:
			if (ed->sel.mode == SEL_WALL)
				add_wall(ed);
			return true;
		case SDL_SCANCODE_DELETE:
			delete_selected(ed);
			return true;
		default:
			return false;
		}

	case SDL_KEYUP:
		switch(misc_handle_scancode(e->key.keysym.scancode)) {
			case SDL_SCANCODE_UP: ed->up = false; return false;
			case SDL_SCANCODE_DOWN: ed->down = false; return false;
			case SDL_SCANCODE_LEFT: ed->left = false; return false;
			case SDL_SCANCODE_RIGHT: ed->right = false; return false;

			case SDL_SCANCODE_A:
				if (ed->rotatedir == 1)
					ed->rotatedir = 0;
				return false;
			case SDL_SCANCODE_D:
				if (ed->rotatedir == -1)
					ed->rotatedir = 0;
				return false;
			default:
				return false;
		}

	default:
		return false;
	}
}

static bool wall_should_be_highlighted(const struct MapEditor *ed, const struct Wall *w)
{
	switch(ed->sel.mode) {
		case SEL_MVWALL: return wall_match(ed->sel.data.mvwall, w);
		case SEL_RESIZE: return wall_linedup(&ed->sel.data.resize.mainwall, w);
		default: return false;
	}
}

struct ToShow {
	struct Wall walls[MAX_WALLS];
	int nwalls;
	struct Ellipsoid els[2 + MAX_ENEMIES];
	int nels;
};

static void show_editor(struct MapEditor *ed)
{
	// static to keep down stack usage
	static struct ToShow behind, select, front;
	behind.nwalls = behind.nels = 0;
	select.nwalls = select.nels = 0;
	front.nwalls = front.nels = 0;

	for (struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit(ed, &ee); ) {
		ee->el.highlighted =
			(ed->sel.mode == SEL_ELLIPSOID || ed->sel.mode == SEL_MVELLIPSOID)
			&& ed->sel.data.eledit == ee;
	}

	struct Wall *hlwall;  // to figure out what's in front or behind highlighted stuff
	switch(ed->sel.mode) {
	case SEL_MVWALL:
		hlwall = ed->sel.data.mvwall;
		break;
	case SEL_RESIZE:
		hlwall = &ed->sel.data.resize.mainwall;
		break;
	case SEL_WALL:
		hlwall = &ed->sel.data.wall;
		break;
	default:
		hlwall = NULL;
		break;
	}

	if (hlwall) {
		for (const struct Wall *w = ed->map->walls; w < ed->map->walls + ed->map->nwalls; w++) {
			if (wall_should_be_highlighted(ed, w))
				select.walls[select.nwalls++] = *w;
			else if (hlwall && wall_side(hlwall, wall_center(w)) == wall_side(hlwall, ed->cam.location) && !wall_linedup(hlwall, w))
				front.walls[front.nwalls++] = *w;
			else
				behind.walls[behind.nwalls++] = *w;
		}

		for (const struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit_const(ed, &ee); ) {
			if (wall_side(hlwall, ee->el.center) == wall_side(hlwall, ed->cam.location))
				front.els[front.nels++] = ee->el;
			else
				behind.els[behind.nels++] = ee->el;
		}
	} else {
		// Everything is "behind", could also use front
		behind.nwalls = ed->map->nwalls;
		for (int i=0; i < behind.nwalls; i++)
			behind.walls[i] = ed->map->walls[i];

		for (const struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit_const(ed, &ee); )
			behind.els[behind.nels++] = ee->el;
	}

	show_all(behind.walls, behind.nwalls, false, behind.els, behind.nels, &ed->cam);
	show_all(select.walls, select.nwalls, true,  select.els, select.nels, &ed->cam);
	if (hlwall) {
		wall_init(hlwall);
		wall_drawborder(hlwall, &ed->cam);
	}
	show_all(front.walls,  front.nwalls,  false, front.els,  front.nels,  &ed->cam);
}

static void add_enemy(void *editorptr)
{
	struct MapEditor *ed = editorptr;
	int m = min(MAX_ENEMIES, ed->map->xsize*ed->map->zsize - 2);
	SDL_assert(ed->map->nenemylocs < m);  // button should be disabled if not

	struct MapCoords hint;
	switch(ed->sel.mode) {
		case SEL_ELLIPSOID:
		case SEL_MVELLIPSOID:
			hint = *ed->sel.data.eledit->loc;
			break;
		case SEL_WALL:
			hint = (struct MapCoords){ ed->sel.data.wall.startx, ed->sel.data.wall.startz };
			break;
		case SEL_MVWALL:
			hint = (struct MapCoords){ ed->sel.data.mvwall->startx, ed->sel.data.mvwall->startz };
			break;
		default:
			hint = (struct MapCoords){0,0};
			break;
	}

	// evaluation order rules troll me
	struct MapCoords c = map_findempty(ed->map, hint);
	ed->map->enemylocs[ed->map->nenemylocs++] = c;
	log_printf("Added enemy, now there are %d enemies", ed->map->nenemylocs);
	map_save(ed->map);
	ed->redraw = true;
}

static void update_button_disableds(struct MapEditor *ed)
{
	int m = min(MAX_ENEMIES, ed->map->xsize*ed->map->zsize - 2);
	SDL_assert(ed->map->nenemylocs <= m);
	if (ed->map->nenemylocs == m)
		ed->addenemybtn.flags |= BUTTON_DISABLED;
	else
		ed->addenemybtn.flags &= ~BUTTON_DISABLED;
}

static void set_to_true(void *ptr)
{
	*(bool *)ptr = true;
}

static void on_done_clicked(void *data)
{
	struct MapEditor *ed = data;
	ed->state = MISC_STATE_CHOOSER;
}

static void confirm_delete(void *ptr)
{
	log_printf("Delete button clicked, entering confirm loop");

	struct MapEditor *ed = ptr;
	SDL_FillRect(ed->cam.surface, NULL, 0);
	SDL_Surface *textsurf = misc_create_text_surface(
		"Are you sure you want to permanently delete this map?",
		(SDL_Color){0xff,0xff,0xff}, 25);

	bool yesclicked = false;
	bool noclicked = false;
	struct Button yesbtn = {
		.text = "Yes, please\ndelete it",
		.destsurf = ed->cam.surface,
		.scancodes = { SDL_SCANCODE_Y },
		.center = { ed->cam.surface->w/2 - button_width(0)/2, ed->cam.surface->h/2 },
		.onclick = set_to_true,
		.onclickdata = &yesclicked,
	};
	struct Button nobtn = {
		.text = "No, don't\ntouch it",
		.scancodes = { SDL_SCANCODE_N, SDL_SCANCODE_ESCAPE },
		.destsurf = ed->cam.surface,
		.center = { ed->cam.surface->w/2 + button_width(0)/2, ed->cam.surface->h/2 },
		.onclick = set_to_true,
		.onclickdata = &noclicked,
	};

	button_show(&yesbtn);
	button_show(&nobtn);
	misc_blit_with_center(textsurf, ed->cam.surface, &(SDL_Point){ ed->cam.surface->w/2, ed->cam.surface->h/4 });

	struct LoopTimer lt = {0};
	while(!yesclicked && !noclicked) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				ed->state = MISC_STATE_QUIT;
				goto out;
			}
			button_handle_event(&e, &yesbtn);
			button_handle_event(&e, &nobtn);
		}
		SDL_assert(ed->wnd);
		SDL_UpdateWindowSurface(ed->wnd);
		looptimer_wait(&lt);
	}

	if (yesclicked) {
		map_delete(ed->maps, ed->nmaps, ed->map - ed->maps);
		ed->state = MISC_STATE_CHOOSER;
	}

out:
	SDL_FreeSurface(textsurf);
}

struct MapEditor *mapeditor_new(SDL_Surface *surf, int ytop)
{
	struct MapEditor *ed = malloc(sizeof(*ed));
	if (!ed)
		log_printf_abort("out of mem");

	*ed = (struct MapEditor){
		.cam = { .surface = surf, .screencentery = ytop, .angle = 0 },
		.donebtn = {
			.text = "Done",
			.destsurf = surf,
			.center = {
				button_width(0)/2,
				button_height(0)/2
			},
			.scancodes = { SDL_SCANCODE_ESCAPE },
			.onclick = on_done_clicked,
			.onclickdata = ed,
		},
		.delmapbtn = {
			.text = "Delete\nthis map",
			.destsurf = surf,
			.center = {
				button_width(0)/2,
				button_height(0)*3/2
			},
			.onclick = confirm_delete,
			.onclickdata = ed,
		},
		.addenemybtn = {
			.text = "Add\nenemy",
			.scancodes = { SDL_SCANCODE_E },
			.destsurf = surf,
			.center = {
				CAMERA_SCREEN_WIDTH - button_width(0)/2,
				button_height(0)/2
			},
			.onclick = add_enemy,
			.onclickdata = ed,
		},
	};

	for (int p = 0; p < 2; p++) {
		ed->playeredits[p].el.xzradius = PLAYER_XZRADIUS;
		ed->playeredits[p].el.yradius = PLAYER_YRADIUS_NOFLAT;
		ed->playeredits[p].el.center.y = PLAYER_YRADIUS_NOFLAT;
		ellipsoid_update_transforms(&ed->playeredits[p].el);
	}
	// Enemies go all the way to max, so don't need to do again if add enemies
	for (int i = 0; i < MAX_ENEMIES; i++) {
		ed->enemyedits[i].el.xzradius = ENEMY_XZRADIUS;
		ed->enemyedits[i].el.yradius = ENEMY_YRADIUS,
		ed->enemyedits[i].el.epic = enemy_getrandomepic();
		ellipsoid_update_transforms(&ed->enemyedits[i].el);
	}
	return ed;
}

void mapeditor_setmaps(struct MapEditor *ed, struct Map *maps, int *nmaps, int mapidx)
{
	SDL_FillRect(ed->cam.surface, NULL, 0);

	ed->map = &maps[mapidx];
	ed->maps = maps;
	ed->nmaps = nmaps;
	ed->redraw = true;
	ed->sel = (struct Selection){ .mode = SEL_NONE };
	ed->state = MISC_STATE_MAPEDITOR;
	ed->rotatedir = 0;

	for (int p = 0; p < 2; p++)
		ed->playeredits[p].loc = &maps[mapidx].playerlocs[p];
	for (int i = 0; i < MAX_ENEMIES; i++)
		ed->enemyedits[i].loc = &maps[mapidx].enemylocs[i];
}

static void show_and_rotate_map_editor(struct MapEditor *ed, bool canedit)
{
	if (ed->rotatedir != 0 || ed->redraw) {
		for (struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit(ed, &ee); ) {
			ee->el.center.x = ee->loc->x + 0.5f;
			ee->el.center.z = ee->loc->z + 0.5f;
		}
		rotate_camera(ed, ed->rotatedir * (canedit ? 3.0f : 1.0f));

		SDL_FillRect(ed->cam.surface, NULL, 0);
		show_editor(ed);
		if (canedit) {
			update_button_disableds(ed);
			button_show(&ed->donebtn);
			button_show(&ed->delmapbtn);
			button_show(&ed->addenemybtn);
		}
	}
	ed->redraw = false;
}

void mapeditor_displayonly_eachframe(struct MapEditor *ed)
{
	ed->rotatedir = -1;  // same direction as players in chooser
	show_and_rotate_map_editor(ed, false);
}

void mapeditor_setplayers(struct MapEditor *ed, const struct EllipsoidPic *plr0pic, const struct EllipsoidPic *plr1pic)
{
	ed->playeredits[0].el.epic = plr0pic;
	ed->playeredits[1].el.epic = plr1pic;
}

enum MiscState mapeditor_run(struct MapEditor *ed, SDL_Window *wnd)
{
	ed->wnd = wnd;
	SDL_Surface *wndsurf = SDL_GetWindowSurface(wnd);
	if (!wndsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());
	SDL_assert(wndsurf == ed->cam.surface);

	struct LoopTimer lt = {0};
	while(1) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				return MISC_STATE_QUIT;
			if (handle_event(ed, &e))
				ed->redraw = true;
			if (ed->state != MISC_STATE_MAPEDITOR)
				return ed->state;
		}

		show_and_rotate_map_editor(ed, true);
		SDL_UpdateWindowSurface(wnd);  // Run every time, in case buttons redraw themselves
		looptimer_wait(&lt);
	}
}
