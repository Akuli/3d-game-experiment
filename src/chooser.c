#include "chooser.h"
#include <math.h>
#include <stddef.h>
#include <SDL2/SDL.h>
#include "button.h"
#include "camera.h"
#include "ellipsoid.h"
#include "log.h"
#include "looptimer.h"
#include "mathstuff.h"
#include "misc.h"
#include "place.h"
#include "player.h"
#include "showall.h"

#define FONT_SIZE 40

#define PLAYER_CHOOSER_HEIGHT ( CAMERA_SCREEN_HEIGHT/2 )
#define PLACE_CHOOSER_HEIGHT (CAMERA_SCREEN_HEIGHT - PLAYER_CHOOSER_HEIGHT)
#define PLACE_CHOOSER_WIDTH (CAMERA_SCREEN_WIDTH - button_width(BUTTON_BIG))

#define ELLIPSOID_XZ_DISTANCE_FROM_ORIGIN 2.0f
#define CAMERA_XZ_DISTANCE_FROM_ORIGIN 5.0f
#define CAMERA_Y 1.6f

static const SDL_Color white_color = { 0xff, 0xff, 0xff, 0xff };

static void calculate_player_chooser_geometry_stuff(
	int leftx, SDL_Rect *preview, SDL_Point *prevbcenter, SDL_Point *nextbcenter, enum ButtonFlags flags)
{
	preview->w = CAMERA_SCREEN_WIDTH/2 - 2*button_width(flags);
	preview->h = PLAYER_CHOOSER_HEIGHT - 2*FONT_SIZE;
	preview->x = leftx + CAMERA_SCREEN_WIDTH/4 - preview->w/2;
	preview->y = FONT_SIZE;

	prevbcenter->x = leftx + button_width(flags)/2;
	nextbcenter->x = leftx + CAMERA_SCREEN_WIDTH/2 - button_width(flags)/2;
	prevbcenter->y = PLAYER_CHOOSER_HEIGHT/2;
	nextbcenter->y = PLAYER_CHOOSER_HEIGHT/2;
}

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

	if (plrch->epic == &player_epics[0] && dir == -1)
		plrch->epic = &player_epics[player_nepics - 1];
	else if (plrch->epic == &player_epics[player_nepics - 1] && dir == +1)
		plrch->epic = &player_epics[0];
	else
		plrch->epic += dir;
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
	SDL_Rect preview;
	SDL_Point prevc, nextc;
	calculate_player_chooser_geometry_stuff(leftx, &preview, &prevc, &nextc, flags);

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
			.center = prevc,
			.onclick = rotate_left,
			.onclickdata = plrch,
		},
		.nextbtn = {
			.flags = flags,
			.imgpath = "assets/arrows/right.png",
			.scancodes = {scnext},
			.destsurf = ch->winsurf,
			.center = nextc,
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
			.epic = &player_epics[i],
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
	show_all(NULL, 0, NULL, ch->ellipsoids, player_nepics, &plrch->cam);
}

static void show_place_chooser_each_frame(struct ChooserPlaceStuff *plcch)
{
	const struct Place *pl = &plcch->places[plcch->placeidx];
	Vec3 placecenter = { pl->xsize/2, 0, pl->zsize/2 };

	float d = hypotf(pl->xsize, pl->zsize);
	Vec3 tocamera = vec3_mul_float((Vec3){0,0.8f,1}, 1.1f*d);
	vec3_apply_matrix(&tocamera, mat3_rotation_xz(plcch->cam.angle));

	// TODO: adjust angle earlier
	plcch->cam.location = vec3_add(placecenter, tocamera);
	plcch->cam.angle -= 0.5f/CAMERA_FPS;   // subtracting makes it spin same direction as ellipsoids
	camera_update_caches(&plcch->cam);

	SDL_FillRect(plcch->cam.surface, NULL, 0);
	show_all(pl->walls, pl->nwalls, NULL, NULL, 0, &plcch->cam);
}

static void set_disabled(struct Button *btn, bool dis)
{
	if (dis)
		btn->flags |= BUTTON_DISABLED;
	else
		btn->flags &= ~BUTTON_DISABLED;
}

static void update_place_chooser_buttons(struct ChooserPlaceStuff *ch)
{
	SDL_assert(0 <= ch->placeidx && ch->placeidx < ch->nplaces);
	set_disabled(&ch->prevbtn, ch->placeidx == 0);
	set_disabled(&ch->nextbtn, ch->placeidx == ch->nplaces-1);
	set_disabled(&ch->editbtn, !ch->places[ch->placeidx].custom);
	button_show(&ch->prevbtn);
	button_show(&ch->nextbtn);
	button_show(&ch->editbtn);
}

static void select_prev_next_place(struct ChooserPlaceStuff *ch, int diff)
{
	ch->placeidx += diff;
	update_place_chooser_buttons(ch);
}
static void select_prev_place(void *ch) { select_prev_next_place(ch, -1); }
static void select_next_place(void *ch) { select_prev_next_place(ch, +1); }

// x and y are mouse location
static void show_or_hide_without_enemies_text(struct Chooser *ch, int x, int y)
{
	SDL_Rect r = ch->withoutenemiesrect;
	bool hovering = (r.x <= x && x <= r.x+r.w && r.y <= y && y <= r.y+r.h);
	if (ch->withoutenemies == hovering)
		return;

	if (hovering)
		SDL_BlitSurface(ch->withoutenemiestxt, NULL, ch->winsurf, &r);
	else
		SDL_FillRect(ch->winsurf, &r, 0);
	ch->withoutenemies = hovering;
}

static enum MiscState handle_event(const SDL_Event *evt, struct Chooser *ch)
{
	if (evt->type == SDL_QUIT)
		return MISC_STATE_QUIT;

	for (int i = 0; i < 2; i++) {
		button_handle_event(evt, &ch->playerch[i].prevbtn);
		button_handle_event(evt, &ch->playerch[i].nextbtn);
	}
	button_handle_event(evt, &ch->placech.prevbtn);
	button_handle_event(evt, &ch->placech.nextbtn);
	button_handle_event(evt, &ch->placech.editbtn);
	button_handle_event(evt, &ch->placech.cpbtn);
	button_handle_event(evt, &ch->bigplaybtn);

	if (evt->type == SDL_MOUSEMOTION)
		show_or_hide_without_enemies_text(ch, evt->motion.x, evt->motion.y);
	if (evt->type == SDL_MOUSEBUTTONUP && ch->withoutenemies)
		return MISC_STATE_PLAY;

	return MISC_STATE_CHOOSER;
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

	enum ButtonFlags placechflags = 0;

	*ch = (struct Chooser){
		.win = win,
		.winsurf = winsurf,
		.bigplaybtn = {
			.text = "Play",
			.flags = BUTTON_BIG,
			.destsurf = winsurf,
			.scancodes = { SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE },
			.center = {
				(PLACE_CHOOSER_WIDTH + CAMERA_SCREEN_WIDTH)/2,
				CAMERA_SCREEN_HEIGHT - PLACE_CHOOSER_HEIGHT/2 - button_height(BUTTON_BIG)/2,
			},
			.onclick = set_to_true,
			// onclickdata is set in chooser_run()
		},
		.placech = {
			.placeidx = 0,
			.cam = {
				.screencentery = -0.55f*PLACE_CHOOSER_HEIGHT,
				.surface = misc_create_cropped_surface(winsurf, (SDL_Rect){
					0,
					CAMERA_SCREEN_HEIGHT - PLACE_CHOOSER_HEIGHT + button_height(placechflags),
					PLACE_CHOOSER_WIDTH,
					PLACE_CHOOSER_HEIGHT - 2*button_height(placechflags)
				}),
				.angle = 0,
			},
			.prevbtn = {
				.imgpath = "assets/arrows/up.png",
				.flags = placechflags,
				.destsurf = winsurf,
				.scancodes = { SDL_SCANCODE_W, SDL_SCANCODE_UP },
				.center = {
					PLACE_CHOOSER_WIDTH/2,
					CAMERA_SCREEN_HEIGHT - PLACE_CHOOSER_HEIGHT + button_height(placechflags)/2,
				},
				.onclick = select_prev_place,
			},
			.nextbtn = {
				.imgpath = "assets/arrows/down.png",
				.flags = placechflags,
				.destsurf = winsurf,
				.scancodes = { SDL_SCANCODE_S, SDL_SCANCODE_DOWN },
				.center = {
					PLACE_CHOOSER_WIDTH/2,
					CAMERA_SCREEN_HEIGHT - button_height(placechflags)/2,
				},
				.onclick = select_next_place,
			},
			.editbtn = {
				.text = "Edit",
				.flags = 0,
				.destsurf = winsurf,
				.scancodes = { SDL_SCANCODE_E },
				.center = {
					(PLACE_CHOOSER_WIDTH + CAMERA_SCREEN_WIDTH)/2,
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
					(PLACE_CHOOSER_WIDTH + CAMERA_SCREEN_WIDTH)/2,
					CAMERA_SCREEN_HEIGHT - button_height(0)/2,
				},
				.onclick = set_to_true,
				// onclickdata is set in chooser_run()
			},
		},
		.withoutenemiestxt = misc_create_text_surface("Practice without enemies", white_color, 10),
	};
	ch->placech.places = place_list(&ch->placech.nplaces);

	ch->withoutenemiesrect = (SDL_Rect){
		20,
		CAMERA_SCREEN_HEIGHT - ch->withoutenemiestxt->h - 20,
		ch->withoutenemiestxt->w,
		ch->withoutenemiestxt->h,
	};

	ch->placech.prevbtn.onclickdata = &ch->placech;
	ch->placech.nextbtn.onclickdata = &ch->placech;

	create_player_ellipsoids(ch);
	setup_player_chooser(ch, 0, SDL_SCANCODE_A, SDL_SCANCODE_D);
	setup_player_chooser(ch, 1, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT);
}

void chooser_destroy(const struct Chooser *ch)
{
	SDL_FreeSurface(ch->playerch[0].cam.surface);
	SDL_FreeSurface(ch->playerch[1].cam.surface);
	SDL_FreeSurface(ch->placech.cam.surface);
	SDL_FreeSurface(ch->withoutenemiestxt);
	free(ch->placech.places);
}

static void show_title_text(SDL_Surface *winsurf)
{
	SDL_Surface *s = misc_create_text_surface("Choose players and place:", white_color, FONT_SIZE);
	misc_blit_with_center(s, winsurf, &(SDL_Point){ winsurf->w/2, FONT_SIZE/2 });
	SDL_FreeSurface(s);
}

static void copy_current_place(struct ChooserPlaceStuff *plcch)
{
	plcch->placeidx = place_copy(&plcch->places, &plcch->nplaces, plcch->placeidx);
}

enum MiscState chooser_run(struct Chooser *ch)
{
	if (ch->placech.placeidx >= ch->placech.nplaces)
		ch->placech.placeidx = ch->placech.nplaces-1;
	update_place_chooser_buttons(&ch->placech);

	bool playclicked = false;
	bool editclicked = false;
	bool cpclicked = false;

	ch->bigplaybtn.onclickdata = &playclicked;
	ch->placech.editbtn.onclickdata = &editclicked;
	ch->placech.cpbtn.onclickdata = &cpclicked;

	SDL_FillRect(ch->winsurf, NULL, 0);
	button_show(&ch->placech.prevbtn);
	button_show(&ch->placech.nextbtn);
	button_show(&ch->placech.editbtn);
	button_show(&ch->placech.cpbtn);
	button_show(&ch->bigplaybtn);
	show_player_chooser_in_beginning(&ch->playerch[0]);
	show_player_chooser_in_beginning(&ch->playerch[1]);
	show_title_text(ch->winsurf);

	struct LoopTimer lt = {0};

	while(1) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			enum MiscState s = handle_event(&e, ch);
			if (s != MISC_STATE_CHOOSER)
				return s;
		}

		if (playclicked)
			return MISC_STATE_PLAY;
		if (editclicked)
			return MISC_STATE_EDITPLACE;
		if (cpclicked) {
			copy_current_place(&ch->placech);
			return MISC_STATE_EDITPLACE;
		}

		rotate_player_ellipsoids(ch->ellipsoids);
		show_player_chooser_each_frame(ch, &ch->playerch[0]);
		show_player_chooser_each_frame(ch, &ch->playerch[1]);
		show_place_chooser_each_frame(&ch->placech);

		SDL_UpdateWindowSurface(ch->win);
		looptimer_wait(&lt);
	}
}
