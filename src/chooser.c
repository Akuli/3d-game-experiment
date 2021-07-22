#include "chooser.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "button.h"
#include "camera.h"
#include "ellipsoid.h"
#include "log.h"
#include "looptimer.h"
#include "mathstuff.h"
#include "misc.h"
#include "map.h"
#include "player.h"
#include "showall.h"

#define FONT_SIZE 40

#define PLAYER_CHOOSER_HEIGHT ( CAMERA_SCREEN_HEIGHT/2 )
#define MAP_CHOOSER_HEIGHT (CAMERA_SCREEN_HEIGHT - PLAYER_CHOOSER_HEIGHT)
#define MAP_CHOOSER_WIDTH (CAMERA_SCREEN_WIDTH - button_width(BUTTON_BIG))

#define ELLIPSOID_XZ_DISTANCE_FROM_ORIGIN 2.0f
#define CAMERA_XZ_DISTANCE_FROM_ORIGIN 5.0f
#define CAMERA_Y 1.6f

static const SDL_Color white_color = { 0xff, 0xff, 0xff, 0xff };

static void update_player_name_display(struct ChooserPlayerStuff *plrch)
{
	SDL_assert(plrch->prevbtn.destsurf == plrch->nextbtn.destsurf);
	SDL_Surface *winsurf = plrch->nextbtn.destsurf;
	SDL_Point center = { plrch->leftx + CAMERA_SCREEN_WIDTH/4, PLAYER_CHOOSER_HEIGHT - FONT_SIZE/2 };

	if (plrch->namew != 0 && plrch->nameh != 0) {
		SDL_Rect r = { center.x - plrch->namew/2, center.y - plrch->nameh/2, plrch->namew, plrch->nameh };
		SDL_FillRect(winsurf, &r, 0);
	}

	char name[100];
	misc_basename_without_extension(plrch->epic->path, name, sizeof name);

	SDL_Surface *s = misc_create_text_surface(name, white_color, FONT_SIZE);
	plrch->namew = s->w;
	plrch->nameh = s->h;
	misc_blit_with_center(s, winsurf, &center);
	SDL_FreeSurface(s);
}

static void rotate_player_chooser(struct ChooserPlayerStuff *plrch, int dir)
{
	SDL_assert(dir == +1 || dir == -1);

	// I considered making plrch->epic point directly into player_epics, so this would be easier.
	// But then it had to be a double pointer and that caused some difficulty elsewhere.
	if (plrch->epic == player_epics[0] && dir == -1)
		plrch->epic = player_epics[player_nepics-1];
	else if (plrch->epic == player_epics[player_nepics-1] && dir == +1)
		plrch->epic = player_epics[0];
	else {
		for (int i = 0; i < player_nepics; i++) {
			if (player_epics[i] == plrch->epic) {
				plrch->epic = player_epics[i+dir];
				break;
			}
		}
	}
	update_player_name_display(plrch);

	// why subtracting: more angle = clockwise from above = left in chooser
	float pi = acosf(-1);
	plrch->anglediff -= dir * (2*pi) / (float)player_nepics;
}

static void rotate_left (void *plrch) { rotate_player_chooser(plrch, -1); }
static void rotate_right(void *plrch) { rotate_player_chooser(plrch, +1); }

static void setup_player_chooser(struct Chooser *ch, int idx, int scprev, int scnext)
{
	int leftx = idx * (ch->winsurf->w/2);

	enum ButtonFlags flags = BUTTON_VERTICAL | BUTTON_SMALL;
	SDL_Rect preview = {
		.w = CAMERA_SCREEN_WIDTH/2 - 2*button_width(flags),
		.h = PLAYER_CHOOSER_HEIGHT - 2*FONT_SIZE,
		.x = leftx + button_width(flags),
		.y = FONT_SIZE,
	};

	float pi = acosf(-1);
	struct ChooserPlayerStuff *plrch = &ch->playerch[idx];
	*plrch = (struct ChooserPlayerStuff){
		.epic = ch->ellipsoids[idx].epic,
		.leftx = leftx,
		.prevbtn = {
			.flags = flags,
			.imgpath = "assets/arrows/left.png",
			.scancodes = {scprev},
			.destsurf = ch->winsurf,
			.center = { leftx + button_width(flags)/2, PLAYER_CHOOSER_HEIGHT/2 },
			.onclick = rotate_left,
			.onclickdata = plrch,
		},
		.nextbtn = {
			.flags = flags,
			.imgpath = "assets/arrows/right.png",
			.scancodes = {scnext},
			.destsurf = ch->winsurf,
			.center = { leftx + CAMERA_SCREEN_WIDTH/2 - button_width(flags)/2, PLAYER_CHOOSER_HEIGHT/2 },
			.onclick = rotate_right,
			.onclickdata = plrch,
		},
		.cam = {
			.screencentery = -preview.h / 10,
			.surface = misc_create_cropped_surface(ch->winsurf, preview),
			.angle = -(2*pi)/player_nepics * idx,
		},
	};

	update_player_name_display(plrch);
	button_show(&plrch->prevbtn);
	button_show(&plrch->nextbtn);
	camera_update_caches(&plrch->cam);
}

static void create_player_ellipsoids(struct Chooser *ch)
{
	SDL_assert(player_nepics <= sizeof(ch->ellipsoids)/sizeof(ch->ellipsoids[0]));
	float pi = acosf(-1);

	for (int i = 0; i < player_nepics; i++) {
		// pi/2 to make first players (i=0 and i=1) look at camera
		float angle = pi/2 - ( i/(float)player_nepics * (2*pi) );

		ch->ellipsoids[i] = (struct Ellipsoid){
			.epic = player_epics[i],
			.center = mat3_mul_vec3(mat3_rotation_xz(angle), (Vec3){ ELLIPSOID_XZ_DISTANCE_FROM_ORIGIN, 0, 0 }),
			.angle = angle,
			.xzradius = PLAYER_XZRADIUS,
			.yradius = PLAYER_YRADIUS_NOFLAT,
		};
		ellipsoid_update_transforms(&ch->ellipsoids[i]);
	}
}

static void rotate_player_ellipsoids(struct Ellipsoid *els)
{
	for (int i = 0; i < player_nepics; i++) {
		els[i].angle += 1.0f / CAMERA_FPS;
		ellipsoid_update_transforms(&els[i]);
	}
}

static float restrict_absolute_value(float val, float maxabs)
{
	SDL_assert(maxabs >= 0);
	if (fabsf(val) > maxabs)
		return copysignf(maxabs, val);
	return val;
}

static void turn_camera(struct ChooserPlayerStuff *plrch)
{
	float turn = restrict_absolute_value(plrch->anglediff, 50.0f / (CAMERA_FPS * player_nepics));
	plrch->cam.angle += turn;
	plrch->anglediff -= turn;

	plrch->cam.location = mat3_mul_vec3(
		mat3_rotation_xz(plrch->cam.angle),
		(Vec3){ 0, CAMERA_Y, CAMERA_XZ_DISTANCE_FROM_ORIGIN });
	camera_update_caches(&plrch->cam);
}

static void show_player_chooser_in_beginning(struct ChooserPlayerStuff *plrch)
{
	button_show(&plrch->prevbtn);
	button_show(&plrch->nextbtn);
	update_player_name_display(plrch);
}

static void show_player_chooser_each_frame(const struct Chooser *ch, struct ChooserPlayerStuff *plrch)
{
	turn_camera(plrch);
	SDL_FillRect(plrch->cam.surface, NULL, 0);
	show_all(NULL, 0, false, ch->ellipsoids, player_nepics, &plrch->cam);
}

static void show_map_chooser_each_frame(struct ChooserMapStuff *mapch)
{
	const struct Map *map = &mapch->maps[mapch->mapidx];
	Vec3 mapcenter = { map->xsize/2, 0, map->zsize/2 };

	float d = hypotf(map->xsize, map->zsize);
	Vec3 tocamera = vec3_mul_float((Vec3){0,0.8f,1}, 1.1f*d);
	vec3_apply_matrix(&tocamera, mat3_rotation_xz(mapch->cam.angle));

	// TODO: see if adjusting angle earlier makes it look somehow better
	mapch->cam.location = vec3_add(mapcenter, tocamera);
	mapch->cam.angle -= 0.5f/CAMERA_FPS;   // subtracting makes it spin same direction as ellipsoids
	camera_update_caches(&mapch->cam);

	SDL_FillRect(mapch->cam.surface, NULL, 0);
	show_all(map->walls, map->nwalls, false, NULL, 0, &mapch->cam);
}

static void set_disabled(struct Button *btn, bool dis)
{
	if (dis)
		btn->flags |= BUTTON_DISABLED;
	else
		btn->flags &= ~BUTTON_DISABLED;
}

static void update_map_chooser_buttons(struct ChooserMapStuff *ch)
{
	SDL_assert(0 <= ch->mapidx && ch->mapidx < ch->nmaps);
	set_disabled(&ch->prevbtn, ch->mapidx == 0);
	set_disabled(&ch->nextbtn, ch->mapidx == ch->nmaps-1);
	set_disabled(&ch->editbtn, !ch->maps[ch->mapidx].custom);
	button_show(&ch->prevbtn);
	button_show(&ch->nextbtn);
	button_show(&ch->editbtn);
}

static void select_prev_next_map(struct ChooserMapStuff *ch, int diff)
{
	ch->mapidx += diff;
	update_map_chooser_buttons(ch);
}
static void select_prev_map(void *ch) { select_prev_next_map(ch, -1); }
static void select_next_map(void *ch) { select_prev_next_map(ch, +1); }

static void handle_event(const SDL_Event *evt, struct Chooser *ch)
{
	for (int i = 0; i < 2; i++) {
		button_handle_event(evt, &ch->playerch[i].prevbtn);
		button_handle_event(evt, &ch->playerch[i].nextbtn);
	}
	button_handle_event(evt, &ch->mapch.prevbtn);
	button_handle_event(evt, &ch->mapch.nextbtn);
	button_handle_event(evt, &ch->mapch.editbtn);
	button_handle_event(evt, &ch->mapch.cpbtn);
	button_handle_event(evt, &ch->bigplaybtn);
}

static void set_to_true(void *ptr)
{
	*(bool *)ptr = true;
}

void chooser_init(struct Chooser *ch, SDL_Window *win)
{
	SDL_Surface *winsurf = SDL_GetWindowSurface(win);
	if (!winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	enum ButtonFlags mapchflags = 0;

	*ch = (struct Chooser){
		.win = win,
		.winsurf = winsurf,
		.bigplaybtn = {
			.text = "Play",
			.flags = BUTTON_BIG,
			.destsurf = winsurf,
			.scancodes = { SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE },
			.center = {
				(MAP_CHOOSER_WIDTH + CAMERA_SCREEN_WIDTH)/2,
				CAMERA_SCREEN_HEIGHT - MAP_CHOOSER_HEIGHT/2 - button_height(BUTTON_BIG)/2,
			},
			.onclick = set_to_true,
			// onclickdata is set in chooser_run()
		},
		.mapch = {
			// maps and nmaps loaded below
			.mapidx = 0,
			.cam = {
				.screencentery = -0.55f*MAP_CHOOSER_HEIGHT,
				.surface = misc_create_cropped_surface(winsurf, (SDL_Rect){
					0,
					CAMERA_SCREEN_HEIGHT - MAP_CHOOSER_HEIGHT + button_height(mapchflags),
					MAP_CHOOSER_WIDTH,
					MAP_CHOOSER_HEIGHT - 2*button_height(mapchflags)
				}),
				.angle = 0,
			},
			.prevbtn = {
				.imgpath = "assets/arrows/up.png",
				.flags = mapchflags,
				.destsurf = winsurf,
				.scancodes = { SDL_SCANCODE_W, SDL_SCANCODE_UP },
				.center = {
					MAP_CHOOSER_WIDTH/2,
					CAMERA_SCREEN_HEIGHT - MAP_CHOOSER_HEIGHT + button_height(mapchflags)/2,
				},
				.onclick = select_prev_map,
			},
			.nextbtn = {
				.imgpath = "assets/arrows/down.png",
				.flags = mapchflags,
				.destsurf = winsurf,
				.scancodes = { SDL_SCANCODE_S, SDL_SCANCODE_DOWN },
				.center = {
					MAP_CHOOSER_WIDTH/2,
					CAMERA_SCREEN_HEIGHT - button_height(mapchflags)/2,
				},
				.onclick = select_next_map,
			},
			.editbtn = {
				.text = "Edit",
				.flags = 0,
				.destsurf = winsurf,
				.scancodes = { SDL_SCANCODE_E },
				.center = {
					(MAP_CHOOSER_WIDTH + CAMERA_SCREEN_WIDTH)/2,
					CAMERA_SCREEN_HEIGHT - button_height(0)*3/2
				},
				.onclick = set_to_true,
				// onclickdata is set in chooser_run()
			},
			.cpbtn = {
				.text = "Copy and\nedit",
				.flags = 0,
				.destsurf = winsurf,
				.scancodes = { SDL_SCANCODE_C },
				.center = {
					(MAP_CHOOSER_WIDTH + CAMERA_SCREEN_WIDTH)/2,
					CAMERA_SCREEN_HEIGHT - button_height(0)/2,
				},
				.onclick = set_to_true,
				// onclickdata is set in chooser_run()
			},
		},
	};
	ch->mapch.maps = map_list(&ch->mapch.nmaps);

	ch->mapch.prevbtn.onclickdata = &ch->mapch;
	ch->mapch.nextbtn.onclickdata = &ch->mapch;

	create_player_ellipsoids(ch);
	setup_player_chooser(ch, 0, SDL_SCANCODE_A, SDL_SCANCODE_D);
	setup_player_chooser(ch, 1, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT);
}

void chooser_destroy(const struct Chooser *ch)
{
	SDL_FreeSurface(ch->playerch[0].cam.surface);
	SDL_FreeSurface(ch->playerch[1].cam.surface);
	SDL_FreeSurface(ch->mapch.cam.surface);
	free(ch->mapch.maps);
}

static void show_title_text(SDL_Surface *winsurf)
{
	SDL_Surface *s = misc_create_text_surface("Choose players and map:", white_color, FONT_SIZE);
	misc_blit_with_center(s, winsurf, &(SDL_Point){ winsurf->w/2, FONT_SIZE/2 });
	SDL_FreeSurface(s);
}

enum MiscState chooser_run(struct Chooser *ch)
{
	if (ch->mapch.mapidx >= ch->mapch.nmaps)
		ch->mapch.mapidx = ch->mapch.nmaps-1;
	update_map_chooser_buttons(&ch->mapch);

	bool playclicked = false;
	bool editclicked = false;
	bool cpclicked = false;

	ch->bigplaybtn.onclickdata = &playclicked;
	ch->mapch.editbtn.onclickdata = &editclicked;
	ch->mapch.cpbtn.onclickdata = &cpclicked;

	SDL_FillRect(ch->winsurf, NULL, 0);
	button_show(&ch->mapch.prevbtn);
	button_show(&ch->mapch.nextbtn);
	button_show(&ch->mapch.editbtn);
	button_show(&ch->mapch.cpbtn);
	button_show(&ch->bigplaybtn);
	show_player_chooser_in_beginning(&ch->playerch[0]);
	show_player_chooser_in_beginning(&ch->playerch[1]);
	show_title_text(ch->winsurf);

	struct LoopTimer lt = {0};

	while(1) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				return MISC_STATE_QUIT;
			handle_event(&e, ch);
		}

		if (playclicked)
			return MISC_STATE_PLAY;
		if (editclicked)
			return MISC_STATE_MAPEDITOR;
		if (cpclicked) {
			ch->mapch.mapidx = map_copy(&ch->mapch.maps, &ch->mapch.nmaps, ch->mapch.mapidx);
			return MISC_STATE_MAPEDITOR;
		}

		rotate_player_ellipsoids(ch->ellipsoids);
		show_player_chooser_each_frame(ch, &ch->playerch[0]);
		show_player_chooser_each_frame(ch, &ch->playerch[1]);
		show_map_chooser_each_frame(&ch->mapch);

		SDL_UpdateWindowSurface(ch->win);
		looptimer_wait(&lt);
	}
}
