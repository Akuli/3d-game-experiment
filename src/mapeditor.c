#include "mapeditor.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include "button.h"
#include "camera.h"
#include "enemy.h"
#include "log.h"
#include "looptimer.h"
#include "map.h"
#include "linalg.h"
#include "max.h"
#include "misc.h"
#include "player.h"
#include "rect3.h"
#include "showall.h"
#include "textentry.h"
#include "wall.h"

struct EllipsoidEdit {
	struct Ellipsoid el;
	struct MapCoords *loc;
};

enum SelectMode { SEL_NONE, SEL_RESIZE, SEL_WALL, SEL_MVWALL, SEL_SQUARE, SEL_MVELLIPSOID };
struct ResizeData {
	struct Wall *walls[MAX_MAPSIZE];
	int nwalls;
	struct Wall mainwall;  // This is the wall whose border is highlighted during resize
	bool negative;   // true if shrinks/expands in negative x or z direction
};
struct Selection {
	enum SelectMode mode;
	union {
		struct MapCoords square;       // SEL_SQUARE
		struct EllipsoidEdit *eledit;  // SEL_MVELLIPSOID
		struct Wall wall;              // SEL_WALL
		struct Wall *mvwall;           // SEL_MVWALL
		struct ResizeData resize;      // SEL_RESIZE
	} data;
};

enum Tool { TOOL_WALL, TOOL_ENEMY };
#define TOOL_COUNT 2

struct MapEditor {
	SDL_Window *wnd;
	enum State state;
	struct Map *map;
	struct EllipsoidEdit playeredits[2];
	struct EllipsoidEdit enemyedits[MAX_ENEMIES];
	struct Camera cam;
	float zoom;
	int rotatedir;
	struct Button donebutton;
	struct Button toolbuttons[TOOL_COUNT];
	enum Tool tool;
	struct Selection sel;
	bool up, down, left, right;   // are arrow keys pressed
	struct TextEntry nameentry;
	bool redraw;
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

	Vec3 tocamera = vec3_mul_float((Vec3){ 0, 0.5f, 0.7f }, d / ed->zoom);
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

static struct EllipsoidEdit *find_ellipsoid_edit_for_square(struct MapEditor *ed, struct MapCoords square)
{
	for (struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit(ed, &ee); ) {
		if (ee->loc->x == square.x && ee->loc->z == square.z)
			return ee;
	}
	return NULL;
}

static bool can_add_wall(const struct MapEditor *ed)
{
	int m = min(
		MAX_WALLS,
		  ed->map->xsize*(ed->map->zsize+1)
		+ ed->map->zsize*(ed->map->xsize+1));
	SDL_assert(ed->map->nwalls <= m);
	return ed->map->nwalls < m;
}

static bool can_add_enemy(const struct MapEditor *ed)
{
	int m = min(MAX_ENEMIES, ed->map->xsize*ed->map->zsize - 2);
	SDL_assert(ed->map->nenemylocs <= m);
	return ed->map->nenemylocs < m;
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
}

static bool mouse_is_on_ellipsoid(const struct Camera *cam, const struct Ellipsoid *el, int x, int y)
{
	if (!ellipsoid_is_visible(el, cam))
		return false;

	int xmin, xmax;
	SDL_Rect bbox = ellipsoid_bbox(el, cam);
	return SDL_PointInRect(&(SDL_Point){x,y}, &bbox)
		&& ellipsoid_xminmax(el, cam, y, &xmin, &xmax)
		&& xmin <= x && x <= xmax;
}

static bool mouse_is_on_wall(const struct Camera *cam, const struct Wall *w, int x, int y)
{
	int xmin, xmax;
	struct Rect3Cache rcache;
	struct Rect3 r = wall_to_rect3(w);
	return rect3_visible_fillcache(&r, cam, &rcache)
		&& rect3_xminmax(&rcache, y, &xmin, &xmax)
		&& xmin <= x && x <= xmax;
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

static bool project_mouse_to_horizontal_plane(
	const struct MapEditor *ed, float h, int mousex, int mousey, float *x, float *z)
{
	// Figure out where on the plane y=h we clicked.
	Vec3 cam2clickdir = {
		// Vector from camera towards clicked direction
		camera_screenx_to_xzr(&ed->cam, mousex),
		camera_screeny_to_yzr(&ed->cam, mousey),
		1
	};
	vec3_apply_matrix(&cam2clickdir, ed->cam.cam2world);

	// p.y should be 1, i.e. top of map
	float dircoeff = -(ed->cam.location.y - h)/cam2clickdir.y;
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
	if (!project_mouse_to_horizontal_plane(ed, 1, mousex, mousey, &fx, &fz))
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
		if (mouse_is_on_wall(&ed->cam, &couldbe[i], mousex, mousey)) {
			*dst = couldbe[i];
			return true;
		}
	}
	return false;
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
		ed->sel = (struct Selection){ .mode = SEL_SQUARE, .data.square = *nearest->loc };
		return;
	}

	// Can we select a wall? When wall tool is not turned on, only select existing walls.
	struct Wall w;
	if (mouse_location_to_wall(ed, &w, mousex, mousey)
		&& wall_is_within_map(&w, ed->map)
		&& (ed->tool == TOOL_WALL || find_wall_from_map(&w, ed->map)))
	{
		ed->sel = (struct Selection){ .mode = SEL_WALL, .data.wall = w };
		return;
	}

	// Can we select a square?
	float x, z;
	if (ed->tool == TOOL_ENEMY
		&& project_mouse_to_horizontal_plane(ed, 0, mousex, mousey, &x, &z)
		&& 0 <= (int)x && (int)x < ed->map->xsize
		&& 0 <= (int)z && (int)z < ed->map->zsize)
	{
		ed->sel = (struct Selection){ .mode = SEL_SQUARE, .data.square = { (int)x, (int)z }};
		return;
	}

	ed->sel = (struct Selection){ .mode = SEL_NONE };
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

static void do_resize(struct MapEditor *ed, int x, int z)
{
	SDL_assert(ed->sel.mode == SEL_RESIZE);
	ed->sel.data.resize.mainwall.startx = x;
	ed->sel.data.resize.mainwall.startz = z;

	keep_wall_within_map(ed, &ed->sel.data.resize.mainwall, true);
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
	}
}

static void finish_resize(struct MapEditor *ed)
{
	SDL_assert(ed->sel.mode == SEL_RESIZE);

	if (ed->sel.data.resize.negative) {
		switch(ed->sel.data.resize.mainwall.dir) {
			case WALL_DIR_XY:
				map_movecontent(ed->map, 0, -ed->sel.data.resize.mainwall.startz);
				ed->map->zsize -= ed->sel.data.resize.mainwall.startz;
				ed->sel.data.resize.mainwall.startz = 0;  // not handled by map_movecontent
				break;
			case WALL_DIR_ZY:
				map_movecontent(ed->map, -ed->sel.data.resize.mainwall.startx, 0);
				ed->map->xsize -= ed->sel.data.resize.mainwall.startx;
				ed->sel.data.resize.mainwall.startx = 0;
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
	ed->sel = (struct Selection){ .mode = SEL_WALL, .data = {.wall = ed->sel.data.resize.mainwall} };
}

static void set_location_of_moving_wall(struct MapEditor *ed, struct Wall w)
{
	SDL_assert(ed->sel.mode == SEL_MVWALL);
	keep_wall_within_map(ed, &w, false);
	if (!find_wall_from_map(&w, ed->map)) {
		// Not going on top of another wall, can move
		*ed->sel.data.mvwall = w;
		map_save(ed->map);
	}
}

static void move_or_select_wall_with_keyboard(struct MapEditor *ed, struct Wall *w, int dx, int dz, bool oppositespressed)
{
	if (oppositespressed)
		w->dir = dz ? WALL_DIR_ZY : WALL_DIR_XY;

	w->startx += dx;
	w->startz += dz;
	keep_wall_within_map(ed, w, false);
}

static void move_selected_ellipsoid(struct MapEditor *ed, int x, int z)
{
	SDL_assert(ed->sel.mode == SEL_MVELLIPSOID);
	clamp(&x, 0, ed->map->xsize-1);
	clamp(&z, 0, ed->map->zsize-1);

	for (const struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit_const(ed, &ee); ) {
		if (ee->loc->x == x && ee->loc->z == z)
			return;
	}

	ed->sel.data.eledit->loc->x = x;
	ed->sel.data.eledit->loc->z = z;
	map_save(ed->map);
}

static struct MapCoords wall_to_square(const struct Map *map, const struct Wall *w, int dx, int dz)
{
	SDL_assert(abs(dx) <= 1 && abs(dz) <= 1);
	struct MapCoords res = {
		w->startx - (dx==-1 && w->dir==WALL_DIR_ZY),
		w->startz - (dz==-1 && w->dir==WALL_DIR_XY),
	};
	clamp(&res.x, 0, map->xsize-1);
	clamp(&res.z, 0, map->zsize-1);
	return res;
}

static struct Wall square_to_wall(struct MapCoords square, int dx, int dz)
{
	SDL_assert(abs(dx) <= 1 && abs(dz) <= 1);
	SDL_assert((dx || dz) && !(dx && dz));
	return (struct Wall){
		.startx = square.x + max(0,dx),
		.startz = square.z + max(0,dz),
		.dir = dx ? WALL_DIR_ZY : WALL_DIR_XY,
	};
}

static void on_arrow_key(struct MapEditor *ed, float angle, bool oppositespressed)
{
	float pi = acosf(-1);
	int rounded90 = (int)roundf(angle / (pi/2));

	int dx = 0, dz = 0;
	switch(((rounded90 % 4) + 4) % 4) {   // modulo is weird in c
		// Trial and error has been used to figure out what to do in each case
		case 0: dz = 1; break;
		case 1: dx = -1; break;
		case 2: dz = -1; break;
		case 3: dx = 1; break;
	}

	switch(ed->sel.mode) {
	case SEL_RESIZE:
		do_resize(ed, ed->sel.data.resize.mainwall.startx + dx, ed->sel.data.resize.mainwall.startz + dz);
		break;
	case SEL_MVELLIPSOID:
		// FIXME: don't move on top of other ellipsoids
		ed->sel.data.eledit->loc->x += dx;
		ed->sel.data.eledit->loc->z += dz;
		clamp(&ed->sel.data.eledit->loc->x, 0, ed->map->xsize-1);
		clamp(&ed->sel.data.eledit->loc->z, 0, ed->map->zsize-1);
		break;
	case SEL_MVWALL:
		// FIXME: don't move on top of other walls
		ed->sel.data.mvwall->startx += dx;
		ed->sel.data.mvwall->startz += dz;
		keep_wall_within_map(ed, ed->sel.data.mvwall, false);
		break;
	case SEL_SQUARE:
		switch(ed->tool) {
		case TOOL_ENEMY:
			ed->sel.data.square.x += dx;
			ed->sel.data.square.z += dz;
			clamp(&ed->sel.data.square.x, 0, ed->map->xsize-1);
			clamp(&ed->sel.data.square.z, 0, ed->map->zsize-1);
			break;
		case TOOL_WALL:
			ed->sel = (struct Selection){
				.mode = SEL_WALL,
				.data.wall = square_to_wall(ed->sel.data.square, dx, dz),
			};
			break;
		}
		break;
	case SEL_WALL:
		switch(ed->tool) {
		case TOOL_ENEMY:
			ed->sel = (struct Selection){
				.mode = SEL_SQUARE,
				.data.square = wall_to_square(ed->map, &ed->sel.data.wall, dx, dz),
			};
			break;
		case TOOL_WALL:
			move_or_select_wall_with_keyboard(ed, &ed->sel.data.wall, dx, dz, oppositespressed);
			break;
		}
		break;
	case SEL_NONE:
		switch(ed->tool) {
		case TOOL_ENEMY:
			ed->sel = (struct Selection){ .mode = SEL_SQUARE, .data.square = {0} };
			break;
		case TOOL_WALL:
			ed->sel = (struct Selection){ .mode = SEL_WALL, .data.wall = {0} };
			break;
		}
		break;
	}
}

static void delete_selected(struct MapEditor *ed)
{
	log_printf("Trying to delete selected item");
	switch(ed->sel.mode) {
		case SEL_SQUARE:
		{
			struct EllipsoidEdit *ee = find_ellipsoid_edit_for_square(ed, ed->sel.data.square);
			// Watch out for ub, comparing pointers to different arrays is undefined
			if (ee && ee != &ed->playeredits[0] && ee != &ed->playeredits[1]) {
				SDL_assert(&ed->enemyedits[0] <= ee && ee < &ed->enemyedits[ed->map->nenemylocs]);
				*ee->loc = ed->map->enemylocs[--ed->map->nenemylocs];
				log_printf("Deleted enemy, now there are %d enemies", ed->map->nenemylocs);
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

static void begin_moving_or_resizing(struct MapEditor *ed)
{
	switch (ed->sel.mode) {
	case SEL_WALL:
		if (is_at_edge(&ed->sel.data.wall, ed->map)) {
			log_printf("Resize begins");
			ed->sel = (struct Selection) {
				.mode = SEL_RESIZE,
				.data.resize = begin_resize(&ed->sel.data.wall, ed->map),
			};
		} else {
			struct Wall *w = find_wall_from_map(&ed->sel.data.wall, ed->map);
			if(w) {
				log_printf("Moving wall begins");
				ed->sel = (struct Selection) { .mode = SEL_MVWALL, .data = { .mvwall = w }};
			}
		}
		break;

	case SEL_SQUARE:
	{
		struct EllipsoidEdit *ee = find_ellipsoid_edit_for_square(ed, ed->sel.data.square);
		if (ee) {
			log_printf("Moving ellipsoid begins");
			ed->sel = (struct Selection){ .mode = SEL_MVELLIPSOID, .data.eledit = ee };
		}
		break;
	}

	default:
		break;
	}
}

static void end_moving_or_resizing(struct MapEditor *ed)
{
	switch(ed->sel.mode) {
	case SEL_RESIZE:
		log_printf("Resize ends");
		finish_resize(ed);
		break;
	case SEL_MVWALL:
		log_printf("Moving a wall ends");
		ed->sel = (struct Selection){ .mode = SEL_WALL, .data.wall = *ed->sel.data.mvwall };
		break;
	case SEL_MVELLIPSOID:
		log_printf("Moving ellipsoid ends");
		struct MapCoords loc = *ed->sel.data.eledit->loc;
		ed->sel = (struct Selection){ .mode = SEL_SQUARE, .data.square = loc };
		break;
	default:
		break;
	}
}

static bool on_mouse_or_enter_released(struct MapEditor *ed)
{
	end_moving_or_resizing(ed);

	if (ed->tool == TOOL_WALL
		&& ed->sel.mode == SEL_WALL
		&& can_add_wall(ed)
		&& !find_wall_from_map(&ed->sel.data.wall, ed->map))
	{
		map_addwall(ed->map, ed->sel.data.wall.startx, ed->sel.data.wall.startz, ed->sel.data.wall.dir);
		log_printf("Added wall, now there are %d walls", ed->map->nwalls);
		map_save(ed->map);
		return true;
	}

	if (ed->tool == TOOL_ENEMY
		&& ed->sel.mode == SEL_SQUARE
		&& can_add_enemy(ed)
		&& !find_ellipsoid_edit_for_square(ed, ed->sel.data.square))
	{
		ed->map->enemylocs[ed->map->nenemylocs++] = ed->sel.data.square;
		log_printf("Added enemy, now there are %d enemies", ed->map->nenemylocs);
		map_save(ed->map);
		return true;
	}

	return false;
}

#define LEFT_CLICK 1
#define RIGHT_CLICK 3

// Returns whether redrawing needed
static bool handle_event(struct MapEditor *ed, const SDL_Event *e)
{
	textentry_handle_event(&ed->nameentry, e);
	if (ed->nameentry.cursor != NULL) {
		ed->sel = (struct Selection){ .mode = SEL_NONE };
		return true;
	}

	float pi = acosf(-1);

	button_handle_event(e, &ed->donebutton);
	for (int i = 0; i < TOOL_COUNT; i++)
		button_handle_event(e, &ed->toolbuttons[i]);

	// If "Yes, delete this map" button was clicked and the map no longer
	// exists, we must avoid handling the click event again
	if (ed->state != STATE_MAPEDITOR)
		return false;

	switch(e->type) {
	case SDL_MOUSEBUTTONDOWN:
		switch(e->button.button) {
		case RIGHT_CLICK:
			delete_selected(ed);
			return true;
		case LEFT_CLICK:
			begin_moving_or_resizing(ed);
			return true;
		default:
			return false;
		}

	case SDL_MOUSEBUTTONUP:
		if (e->button.button != LEFT_CLICK)
			return false;
		on_mouse_or_enter_released(ed);
		return true;

	case SDL_MOUSEMOTION:
	{
		switch(ed->sel.mode) {
		case SEL_MVELLIPSOID:
		case SEL_RESIZE:
		{
			float xf, zf;
			if (project_mouse_to_horizontal_plane(ed, 1, e->button.x, e->button.y, &xf, &zf)) {
				if (ed->sel.mode == SEL_MVELLIPSOID)
					move_selected_ellipsoid(ed, (int)floorf(xf), (int)floorf(zf));
				else if (ed->sel.mode == SEL_RESIZE)
					do_resize(ed, (int)roundf(xf), (int)roundf(zf));
				else
					log_printf_abort("the impossible happened");
			}
			break;
		}
		case SEL_MVWALL:
		{
			struct Wall w;
			if (mouse_location_to_wall(ed, &w, e->button.x, e->button.y))
				set_location_of_moving_wall(ed, w);
			break;
		}
		default:
			select_by_mouse_coords(ed, e->button.x, e->button.y);
			break;
		}
		return true;
	}

	case SDL_KEYDOWN:
		switch(normalize_scancode(e->key.keysym.scancode)) {
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
			begin_moving_or_resizing(ed);
			return true;
		case SDL_SCANCODE_DELETE:
			delete_selected(ed);
			return true;
		default:
			return false;
		}

	case SDL_KEYUP:
		switch(normalize_scancode(e->key.keysym.scancode)) {
			case SDL_SCANCODE_UP: ed->up = false; return false;
			case SDL_SCANCODE_DOWN: ed->down = false; return false;
			case SDL_SCANCODE_LEFT: ed->left = false; return false;
			case SDL_SCANCODE_RIGHT: ed->right = false; return false;

			case SDL_SCANCODE_RETURN:
				on_mouse_or_enter_released(ed);
				return true;
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

static void show_editor(struct MapEditor *ed)
{
	struct EllipsoidEdit *highlightee;
	switch(ed->sel.mode) {
	case SEL_SQUARE:
		highlightee = find_ellipsoid_edit_for_square(ed, ed->sel.data.square);
		break;
	case SEL_MVELLIPSOID:
		highlightee = ed->sel.data.eledit;
		break;
	default:
		highlightee = NULL;
		break;
	}

	for (struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit(ed, &ee); )
		ee->el.highlighted = (ee==highlightee);

	int highlight[MAX_WALLS + 1];
	int highlightidx = 0;
	for (int i = 0; i < ed->map->nwalls; i++) {
		if (wall_should_be_highlighted(ed, &ed->map->walls[i]))
			highlight[highlightidx++] = i;
	}
	highlight[highlightidx] = -1;

	struct Ellipsoid els[2 + MAX_ENEMIES];
	int nels = 0;
	for (const struct EllipsoidEdit *ee = NULL; next_ellipsoid_edit_const(ed, &ee); )
		els[nels++] = ee->el;

	show_all(ed->map->walls, ed->map->nwalls, highlight, els, nels, &ed->cam);

	struct Wall *borderwall;
	switch(ed->sel.mode) {
	case SEL_MVWALL:
		borderwall = ed->sel.data.mvwall;
		break;
	case SEL_RESIZE:
		borderwall = &ed->sel.data.resize.mainwall;
		break;
	case SEL_WALL:
		borderwall = &ed->sel.data.wall;
		break;
	default:
		borderwall = NULL;
		break;
	}
	if (borderwall) {
		struct Rect3 r = wall_to_rect3(borderwall);
		rect3_drawborder(&r, &ed->cam);
	}

	if (ed->sel.mode == SEL_SQUARE) {
		struct Rect3 r = { .corners = {
			{ ed->sel.data.square.x, 0, ed->sel.data.square.z },
			{ ed->sel.data.square.x, 0, ed->sel.data.square.z+1 },
			{ ed->sel.data.square.x+1, 0, ed->sel.data.square.z+1 },
			{ ed->sel.data.square.x+1, 0, ed->sel.data.square.z },
		}};
		rect3_drawborder(&r, &ed->cam);
	}
}

static void on_tool_changed(struct MapEditor *ed, enum Tool tool)
{
	log_printf("Changing tool to %d", tool);
	end_moving_or_resizing(ed);
	ed->tool = tool;
	for (int t = 0; t < TOOL_COUNT; t++) {
		if (t != tool)
			ed->toolbuttons[t].flags &= ~BUTTON_PRESSED;
	}
	ed->redraw = true;
}

static void on_wall_button_clicked(void *edptr) { on_tool_changed(edptr, TOOL_WALL); }
static void on_enemy_button_clicked(void *edptr) { on_tool_changed(edptr, TOOL_ENEMY); }

static void on_done_clicked(void *data)
{
	struct MapEditor *ed = data;
	ed->state = STATE_CHOOSER;
}

struct MapEditor *mapeditor_new(SDL_Surface *surf, int ytop, float zoom)
{
	struct MapEditor *ed = malloc(sizeof(*ed));
	if (!ed)
		log_printf_abort("out of mem");

	enum ButtonFlags bf = BUTTON_THICK | BUTTON_VERTICAL | BUTTON_STAYPRESSED;
	*ed = (struct MapEditor){
		.zoom = zoom,
		.cam = { .surface = surf, .screencentery = ytop, .angle = 0 },
		.donebutton = {
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
		.toolbuttons = {
			[TOOL_WALL] = {
				.imgpath = "assets/buttonpics/wall.png",
				.flags = bf | BUTTON_PRESSED,
				.scancodes = { SDL_SCANCODE_W },
				.destsurf = surf,
				.center = {
					CAMERA_SCREEN_WIDTH - button_width(BUTTON_THICK)/2,
					button_height(bf)/2
				},
				.onclick = on_wall_button_clicked,
				.onclickdata = ed,
			},
			[TOOL_ENEMY] = {
				.imgpath = "assets/buttonpics/enemy.png",
				.flags = bf,
				.scancodes = { SDL_SCANCODE_E },
				.destsurf = surf,
				.center = {
					CAMERA_SCREEN_WIDTH - button_width(BUTTON_THICK)/2,
					button_height(bf)*3/2
				},
				.onclick = on_enemy_button_clicked,
				.onclickdata = ed,
			},
		},
		.nameentry = {
			.surf = surf,
			.rect = {
				.x = button_width(0),
				.y = 0,
				.w = surf->w - 2*button_width(0),
				.h = button_height(0),
			},
			// text and change callback assigned later
			.maxlen = sizeof(((struct Map *)NULL)->name) - 1,
			.fontsz = 32,
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

static void name_changed_callback(void *ptr)
{
	map_save(ptr);
}

void mapeditor_setmap(struct MapEditor *ed, struct Map *map)
{
	SDL_FillRect(ed->cam.surface, NULL, 0);

	ed->map = map;
	ed->redraw = true;
	ed->sel = (struct Selection){ .mode = SEL_NONE };
	ed->state = STATE_MAPEDITOR;
	ed->rotatedir = 0;

	ed->nameentry.text = map->name;
	ed->nameentry.redraw = true;
	ed->nameentry.changecb = name_changed_callback;
	ed->nameentry.changecbdata = map;

	for (int p = 0; p < 2; p++)
		ed->playeredits[p].loc = &map->playerlocs[p];
	for (int i = 0; i < MAX_ENEMIES; i++)
		ed->enemyedits[i].loc = &map->enemylocs[i];
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
			button_show(&ed->donebutton);
			for (int i = 0; i < TOOL_COUNT; i++)
				button_show(&ed->toolbuttons[i]);
			ed->nameentry.redraw = true;  // because entire surface was cleared above
		}
	}
	ed->redraw = false;

	if (canedit)
		textentry_show(&ed->nameentry);
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

enum State mapeditor_run(struct MapEditor *ed, SDL_Window *wnd)
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
				return STATE_QUIT;
			if (handle_event(ed, &e))
				ed->redraw = true;
			if (ed->state != STATE_MAPEDITOR)
				return ed->state;
		}

		show_and_rotate_map_editor(ed, true);
		SDL_UpdateWindowSurface(wnd);  // Run every time, in case buttons redraw themselves
		looptimer_wait(&lt);
	}
}
