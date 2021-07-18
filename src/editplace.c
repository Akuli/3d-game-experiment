#include "editplace.h"
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
#include "gameover.h"

enum SelectMode { SEL_NONE, SEL_PLAYER, SEL_WALL, SEL_MOVINGWALL, SEL_RESIZE };
struct ResizeData {
	struct Wall *walls[MAX_PLACE_SIZE];
	int nwalls;
	struct Wall mainwall;  // This is the wall whose border is highlighted during resize
	bool negative;   // true if shrinks/expands in negative x or z direction
};
struct Selection {
	enum SelectMode mode;
	union {
		int playeridx;             // SEL_PLAYER
		struct Wall wall;          // SEL_WALL
		struct Wall *mvwall;       // SEL_MOVINGWALL
		struct ResizeData resize;  // SEL_RESIZE
	} data;
};

struct PlaceEditor {
	enum MiscState state;
	struct Place *place;
	struct Ellipsoid playerels[2];
	struct Camera cam;
	int rotatedir;
	struct Button deletebtn, donebtn;
	struct Selection sel;
	bool mousemoved;
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

static struct Wall *find_wall_from_place(struct PlaceEditor *pe, const struct Wall *w)
{
	for (int i = 0; i < pe->place->nwalls; i++) {
		if (wall_match(&pe->place->walls[i], w))
			return &pe->place->walls[i];
	}
	return NULL;
}

static bool add_wall(struct PlaceEditor *pe)
{
	if (pe->sel.mode != SEL_WALL)
		return false;

	SDL_assert(pe->place->nwalls <= MAX_WALLS);
	if (pe->place->nwalls == MAX_WALLS) {
		log_printf("hitting max number of walls, can't add more");
		return false;
	}

	if (find_wall_from_place(pe, &pe->sel.data.wall))
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

static void delete_wall(struct PlaceEditor *pe)
{
	if (pe->sel.mode != SEL_WALL)
		return;

	struct Wall *w = find_wall_from_place(pe, &pe->sel.data.wall);
	if (w && !is_at_edge(w, pe->place)) {
		*w = pe->place->walls[--pe->place->nwalls];
		log_printf("Deleted wall, now there are %d walls", pe->place->nwalls);
		place_save(pe->place);
	}
}

static void keep_wall_within_place(const struct PlaceEditor *pe, struct Wall *w)
{
	int xmin = 0, xmax = pe->place->xsize;
	int zmin = 0, zmax = pe->place->zsize;

	switch(pe->sel.mode) {
	case SEL_RESIZE:
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
		break;

	case SEL_WALL:
		/*
		// If you move selection beyond edge, it changes direction so it's parallel to the edge
		// No "else if" so that if this is called many times, it doesn't constantly change
		// TODO: allow flipping a wall around by smashing it to edge
		if (w->dir == WALL_DIR_XY && (w->startx < 0 || w->startx >= pe->place->xsize))
			w->dir = WALL_DIR_ZY;
		if (w->dir == WALL_DIR_ZY && (w->startz < 0 || w->startz >= pe->place->zsize))
			w->dir = WALL_DIR_XY;
		*/
		break;

	default:
		break;
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
		if (wall_side(w, pe->cam.location) != wall_side(w, el->center)
			&& mouse_is_on_wall(&pe->cam, w, x, y))
		{
			return false;
		}
	}
	return true;
}

static void on_mouse_move(struct PlaceEditor *pe, int mousex, int mousey)
{
	if (pe->sel.mode != SEL_MOVINGWALL && pe->sel.mode != SEL_RESIZE) {
		bool on0 = mouse_is_on_ellipsoid_with_no_walls_between(pe, &pe->playerels[0], mousex, mousey);
		bool on1 = mouse_is_on_ellipsoid_with_no_walls_between(pe, &pe->playerels[1], mousex, mousey);
		if (on0 && on1) {
			// Select closer to camera
			float d0 = vec3_lengthSQUARED(vec3_sub(pe->playerels[0].center, pe->cam.location));
			float d1 = vec3_lengthSQUARED(vec3_sub(pe->playerels[1].center, pe->cam.location));
			on0 = (d0 < d1);
			on1 = (d0 > d1);
		}
		if (on0 || on1) {
			pe->sel = (struct Selection){ .mode = SEL_PLAYER, .data = { .playeridx = on0 ? 0 : 1 } };
			return;
		}
	}

	// Top of place is at plane y=1. Figure out where on it we clicked
	Vec3 cam2clickdir = {
		// Vector from camera towards clicked direction
		camera_screenx_to_xzr(&pe->cam, mousex),
		camera_screeny_to_yzr(&pe->cam, mousey),
		1
	};
	vec3_apply_matrix(&cam2clickdir, pe->cam.cam2world);

	// cam->location + dircoeff*cam2clickdir has y coordinate 1
	float dircoeff = -(pe->cam.location.y - 1)/cam2clickdir.y;
	Vec3 p = vec3_add(pe->cam.location, vec3_mul_float(cam2clickdir, dircoeff));  // on the plane

	if (!isfinite(p.x) || !isfinite(p.z))
		return;

	if (pe->sel.mode != SEL_MOVINGWALL && pe->sel.mode != SEL_RESIZE) {
		// Allow off by a little bit so you can select edge walls
		float tol = 1;
		if (p.x < -tol || p.x > pe->place->xsize + tol || p.z < -tol || p.z > pe->place->zsize + tol)
			return;
	}

	struct Wall w;
	switch(pe->sel.mode) {
		case SEL_MOVINGWALL: w.dir = pe->sel.data.mvwall->dir; break;
		case SEL_RESIZE: w.dir = pe->sel.data.resize.mainwall.dir; break;
		case SEL_WALL: w.dir = pe->sel.data.wall.dir; break;
		default: w.dir = WALL_DIR_XY; break;  // TODO: remember dir better
	}

	switch(w.dir) {
	case WALL_DIR_XY:
		w.startz = (int)(cam2clickdir.z>0 ? floorf(p.z) : ceilf(p.z)); // towards camera
		w.startx = (int)floorf(p.x);
		break;
	case WALL_DIR_ZY:
		w.startx = (int)(cam2clickdir.x>0 ? floorf(p.x) : ceilf(p.x)); // towards camera
		w.startz = (int)floorf(p.z);
		break;
	}

	switch(pe->sel.mode) {
		case SEL_MOVINGWALL:
			keep_wall_within_place(pe, &w);
			if (!find_wall_from_place(pe, &w)) {
				// Not going on top of another wall, can move
				*pe->sel.data.mvwall = w;
				wall_init(pe->sel.data.mvwall);
				place_save(pe->place);
			}
			break;

		case SEL_RESIZE:
			keep_wall_within_place(pe, &w);
			wall_init(&w);
			pe->sel.data.resize.mainwall = w;
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
			break;

		case SEL_NONE:
		case SEL_PLAYER:
		case SEL_WALL:
			keep_wall_within_place(pe, &w);
			pe->sel = (struct Selection){ .mode = SEL_WALL, .data = { .wall = w }};
			break;
	}
}

static void move_towards_angle(struct PlaceEditor *pe, float angle)
{
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
	case SEL_WALL:
		// Check if we are going towards a player
		if ((pe->sel.data.wall.dir == WALL_DIR_ZY && dx) ||
			(pe->sel.data.wall.dir == WALL_DIR_XY && dz))
		{
			int px = pe->sel.data.wall.startx + min(0, dx);
			int pz = pe->sel.data.wall.startz + min(0, dz);
			for (int p=0; p<2; p++) {
				if (pe->place->playerlocs[p].x == px && pe->place->playerlocs[p].z == pz) {
					pe->sel.mode = SEL_PLAYER;
					pe->sel.data.playeridx = p;
					return;
				}
			}
		}

		pe->sel.data.wall.startx += dx;
		pe->sel.data.wall.startz += dz;
		keep_wall_within_place(pe, &pe->sel.data.wall);
		break;

	case SEL_PLAYER:
		{
			// Select wall near player
			int p = pe->sel.data.playeridx;
			pe->sel = (struct Selection) {
				.mode = SEL_WALL,
				.data = {
					.wall = {
						.dir = (dx ? WALL_DIR_ZY : WALL_DIR_XY),
						.startx = pe->place->playerlocs[p].x + max(0, dx),
						.startz = pe->place->playerlocs[p].z + max(0, dz),
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
	log_printf("Resize ends");
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
		if (pe->sel.mode == SEL_WALL) {
			if (is_at_edge(&pe->sel.data.wall, pe->place)) {
				struct Wall w = pe->sel.data.wall;
				pe->sel = (struct Selection) {
					.mode = SEL_RESIZE,
					.data = { .resize = begin_resize(&w, pe->place) },
				};
			} else {
				// TODO move
			}
		}
		return true;

	case SDL_MOUSEMOTION:
		pe->mousemoved = true;
		on_mouse_move(pe, e->button.x, e->button.y);
		return true;

	case SDL_MOUSEBUTTONUP:
		switch(pe->sel.mode) {
		case SEL_RESIZE:
			finish_resize(pe);
			break;
	/*
		case DND_MOVING:
			log_printf("Moving a wall ends, mousemoved = %d", pe->mousemoved);
			if (!pe->mousemoved)
				delete_wall(pe);
			break;
		case DND_NONE:
			log_printf("Click ends");
			add_wall(pe);
			break;
	*/
		}
		pe->sel.mode = SEL_NONE;
		return true;

	case SDL_KEYDOWN:
		switch(pe->sel.mode) {
			case SEL_NONE:
			case SEL_WALL:
			case SEL_PLAYER:
				break;
			default:
				return false;  // TODO: more keyboard functionality
		}

		switch(misc_handle_scancode(e->key.keysym.scancode)) {
		case SDL_SCANCODE_DOWN:
			move_towards_angle(pe, pe->cam.angle);
			return true;
		case SDL_SCANCODE_LEFT:
			move_towards_angle(pe, pe->cam.angle + pi/2);
			return true;
		case SDL_SCANCODE_UP:
			move_towards_angle(pe, pe->cam.angle + pi);
			return true;
		case SDL_SCANCODE_RIGHT:
			move_towards_angle(pe, pe->cam.angle + 3*pi/2);
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
		if (pe->sel.mode != SEL_NONE && pe->sel.mode != SEL_WALL)
			return false;  // TODO: more keyboard functionality

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

static bool wall_should_be_highlighted(const struct PlaceEditor *pe, const struct Wall *w)
{
	switch(pe->sel.mode) {
	case SEL_MOVINGWALL:
		return wall_match(pe->sel.data.mvwall, w);
	case SEL_RESIZE:
		return wall_linedup(&pe->sel.data.resize.mainwall, w);
	default:
		return false;
	}
}

// TODO: should this go to showall.h?
struct ToShow {
	struct Wall walls[MAX_WALLS];
	int nwalls;
	struct Ellipsoid els[2];
	int nels;
};

static void show_editor(struct PlaceEditor *pe)
{
	// static to keep down stack usage
	static struct ToShow behind, select, front;
	behind.nwalls = behind.nels = 0;
	select.nwalls = select.nels = 0;
	front.nwalls = front.nels = 0;

	struct Wall *hlwall;
	switch(pe->sel.mode) {
	case SEL_MOVINGWALL:
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

	for (const struct Wall *w = pe->place->walls; w < pe->place->walls + pe->place->nwalls; w++) {
		if (wall_should_be_highlighted(pe, w))
			select.walls[select.nwalls++] = *w;
		else if (hlwall && wall_side(hlwall, wall_center(w)) == wall_side(hlwall, pe->cam.location) && !wall_linedup(hlwall, w))
			front.walls[front.nwalls++] = *w;
		else
			behind.walls[behind.nwalls++] = *w;
	}

	for (int p=0; p<2; p++) {
		if (hlwall && wall_side(hlwall, pe->playerels[p].center) == wall_side(hlwall, pe->cam.location))
			front.els[front.nels++] = pe->playerels[p];
		else
			behind.els[behind.nels++] = pe->playerels[p];
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
	const struct EllipsoidPic *plr1pic, const struct EllipsoidPic *plr2pic)
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
		.playerels = {
			{ .angle = 0, .epic = plr1pic },
			{ .angle = 0, .epic = plr2pic },
		},
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

	for (int p=0; p<2; p++) {
		pe.playerels[p].xzradius = PLAYER_XZRADIUS;
		pe.playerels[p].yradius = PLAYER_YRADIUS_NOFLAT;
		ellipsoid_update_transforms(&pe.playerels[p]);
	}
	deldata.editor = &pe;
	rotate_camera(&pe, 0);

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
			for (int p = 0; p < 2; p++) {
				pe.playerels[p].center = (Vec3){
					pe.place->playerlocs[p].x + 0.5f,
					PLAYER_YRADIUS_NOFLAT,
					pe.place->playerlocs[p].z + 0.5f,
				};
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
