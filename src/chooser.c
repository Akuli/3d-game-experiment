#include "chooser.h"
#include "../generated/filelist.h"
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
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define FONT_SIZE 40

#define PLAYER_CHOOSER_HEIGHT ( CAMERA_SCREEN_HEIGHT/2 )
#define PLACE_CHOOSER_HEIGHT (CAMERA_SCREEN_HEIGHT - PLAYER_CHOOSER_HEIGHT)
#define PLACE_CHOOSER_WIDTH (CAMERA_SCREEN_WIDTH - PLAY_BUTTON_AREA_WIDTH)
#define PLAY_BUTTON_AREA_WIDTH (CAMERA_SCREEN_WIDTH * 2/5)  // TODO: add something to button.h

#define ELLIPSOID_XZ_DISTANCE_FROM_ORIGIN 2.0f
#define CAMERA_XZ_DISTANCE_FROM_ORIGIN 5.0f
#define CAMERA_Y 1.6f

static const SDL_Color white_color = { 0xff, 0xff, 0xff, 0xff };

static void calculate_player_chooser_geometry_stuff(
	int leftx, SDL_Rect *preview, SDL_Point *prevbcenter, SDL_Point *nextbcenter)
{
	preview->w = CAMERA_SCREEN_WIDTH/2 - 2*BUTTON_SIZE_SMALL;
	preview->h = PLAYER_CHOOSER_HEIGHT - 2*FONT_SIZE;
	preview->x = leftx + CAMERA_SCREEN_WIDTH/4 - preview->w/2;
	preview->y = FONT_SIZE;

	prevbcenter->x = leftx + BUTTON_SIZE_SMALL/2;
	nextbcenter->x = leftx + CAMERA_SCREEN_WIDTH/2 - BUTTON_SIZE_SMALL/2;
	prevbcenter->y = PLAYER_CHOOSER_HEIGHT/2;
	nextbcenter->y = PLAYER_CHOOSER_HEIGHT/2;
}

static void update_player_name_display(struct ChooserPlayerStuff *plrch)
{
	assert(plrch->prevbtn.destsurf == plrch->nextbtn.destsurf);
	SDL_Surface *winsurf = plrch->nextbtn.destsurf;
	SDL_Point center = { plrch->leftx + CAMERA_SCREEN_WIDTH/4, PLAYER_CHOOSER_HEIGHT - FONT_SIZE/2 };

	if (plrch->namew != 0 && plrch->nameh != 0) {
		SDL_Rect r = { center.x - plrch->namew/2, center.y - plrch->nameh/2, plrch->namew, plrch->nameh };
		SDL_FillRect(winsurf, &r, 0);
	}

	char name[100];
	player_epic_name(plrch->epic, name, sizeof name);

	SDL_Surface *s = misc_create_text_surface(name, white_color, FONT_SIZE);
	plrch->namew = s->w;
	plrch->nameh = s->h;
	misc_blit_with_center(s, winsurf, &center);
	SDL_FreeSurface(s);
}

static void rotate_player_chooser(struct ChooserPlayerStuff *plrch, int dir)
{
	assert(dir == +1 || dir == -1);

	plrch->epic += dir;
	if (plrch->epic == player_get_epics(NULL) - 1)
		plrch->epic += FILELIST_NPLAYERS;
	else if (plrch->epic == player_get_epics(NULL) + FILELIST_NPLAYERS)
		plrch->epic -= FILELIST_NPLAYERS;
	update_player_name_display(plrch);

	// why subtracting: more angle = clockwise from above = left in chooser
	float pi = acosf(-1);
	plrch->anglediff -= dir * (2*pi) / (float)FILELIST_NPLAYERS;
}

static void rotate_left (void *plrch) { rotate_player_chooser(plrch, -1); }
static void rotate_right(void *plrch) { rotate_player_chooser(plrch, +1); }

static void setup_player_chooser(struct Chooser *ch, int idx, int scprev, int scnext)
{
	int leftx = idx * (ch->winsurf->w/2);

	SDL_Rect preview;
	SDL_Point prevc, nextc;
	calculate_player_chooser_geometry_stuff(leftx, &preview, &prevc, &nextc);

	float pi = acosf(-1);
	struct ChooserPlayerStuff *plrch = &ch->playerch[idx];
	*plrch = (struct ChooserPlayerStuff){
		.epic = ch->ellipsoids[idx].epic,
		.leftx = leftx,
		.prevbtn = {
			.imgpath = "arrows/left.png",
			.scancode = scprev,
			.destsurf = ch->winsurf,
			.center = prevc,
			.onclick = rotate_left,
			.onclickdata = plrch,
		},
		.nextbtn = {
			.imgpath = "arrows/right.png",
			.scancode = scnext,
			.destsurf = ch->winsurf,
			.center = nextc,
			.onclick = rotate_right,
			.onclickdata = plrch,
		},
		.cam = {
			.screencentery = -preview.h / 10,
			.surface = misc_create_cropped_surface(ch->winsurf, preview),
			.angle = -(2*pi)/FILELIST_NPLAYERS * idx,
		},
	};

	update_player_name_display(plrch);
	button_show(&plrch->prevbtn);
	button_show(&plrch->nextbtn);
	camera_update_caches(&plrch->cam);
}

static void turn_camera(struct ChooserPlayerStuff *plrch)
{
	float maxturn = 50.0f / (CAMERA_FPS * FILELIST_NPLAYERS);
	assert(maxturn > 0);
	float turn = max(-maxturn, min(maxturn, plrch->anglediff));

	plrch->cam.angle += turn;
	plrch->anglediff -= turn;

	plrch->cam.location = mat3_mul_vec3(
		mat3_rotation_xz(plrch->cam.angle),
		(Vec3){ 0, CAMERA_Y, CAMERA_XZ_DISTANCE_FROM_ORIGIN });
	camera_update_caches(&plrch->cam);
}

// returns false when game should quit
static bool handle_event(const SDL_Event *evt, struct Chooser *ch)
{
	if (evt->type == SDL_QUIT)
		return false;

	for (int i = 0; i < 2; i++) {
		button_handle_event(evt, &ch->playerch[i].prevbtn);
		button_handle_event(evt, &ch->playerch[i].nextbtn);
	}
	button_handle_event(evt, &ch->placech.prevbtn);
	button_handle_event(evt, &ch->placech.nextbtn);
	button_handle_event(evt, &ch->bigplaybtn);
	return true;
}

static void show_player_chooser_in_beginning(struct ChooserPlayerStuff *plrch)
{
	button_show(&plrch->prevbtn);
	button_show(&plrch->nextbtn);
	update_player_name_display(plrch);
}

static void show_player_chooser_each_frame(const struct Chooser *ch, const struct ChooserPlayerStuff *plrch)
{
	SDL_FillRect(plrch->cam.surface, NULL, 0);
	show_all(NULL, 0, ch->ellipsoids, sizeof(ch->ellipsoids)/sizeof(ch->ellipsoids[0]), &plrch->cam);
}

static void create_player_ellipsoids(struct Ellipsoid *arr, const SDL_PixelFormat *fmt)
{
	float pi = acosf(-1);

	for (int i = 0; i < FILELIST_NPLAYERS; i++) {
		// pi/2 to make first players (i=0 and i=1) look at camera
		float angle = pi/2 - ( i/(float)FILELIST_NPLAYERS * (2*pi) );

		arr[i] = (struct Ellipsoid){
			.epic = &player_get_epics(fmt)[i],
			.center = mat3_mul_vec3(mat3_rotation_xz(angle), (Vec3){ ELLIPSOID_XZ_DISTANCE_FROM_ORIGIN, 0, 0 }),
			.angle = angle,
			.xzradius = PLAYER_XZRADIUS,
			.yradius = PLAYER_YRADIUS_NOFLAT,
		};
		ellipsoid_update_transforms(&arr[i]);
	}
}

static void show_the_place(struct ChooserPlaceStuff *plcch)
{
	// for some reason, subtracting makes it spin same direction as players
	plcch->cam.angle -= 0.5f/CAMERA_FPS;

	Vec3 placecenter = { plcch->pl->xsize/2, 0, plcch->pl->zsize/2 };

	float d = hypotf(plcch->pl->xsize, plcch->pl->zsize);
	Vec3 tocamera = vec3_mul_float((Vec3){0,0.8f,1}, 1.1f*d);
	vec3_apply_matrix(&tocamera, mat3_rotation_xz(plcch->cam.angle));

	plcch->cam.location = vec3_add(placecenter, tocamera);
	camera_update_caches(&plcch->cam);

	SDL_FillRect(plcch->cam.surface, NULL, 0);
	show_all(plcch->pl->walls, plcch->pl->nwalls, NULL, 0, &plcch->cam);
}

static void select_prev_next_place(struct ChooserPlaceStuff *ch, int diff)
{
	const struct Place *start = place_list();
	const struct Place *end = start + FILELIST_NPLACES;
	assert(start <= ch->pl && ch->pl < end);
	assert(diff == +1 || diff == -1);

	const struct Place *newpl = ch->pl + diff;
	if (start <= newpl && newpl < end)
		ch->pl = newpl;
}

static void select_prev_place(void *ch) { select_prev_next_place(ch, -1); }
static void select_next_place(void *ch) { select_prev_next_place(ch, +1); }

static void on_play_button_clicked(void *ptr)
{
	*(bool *)ptr = true;
}

void chooser_init(struct Chooser *ch, SDL_Window *win)
{
	SDL_Surface *winsurf = SDL_GetWindowSurface(win);
	if (!winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	*ch = (struct Chooser){
		.win = win,
		.winsurf = winsurf,
		.bigplaybtn = {
			.text = "Play",
			.flags = BUTTON_BIG | BUTTON_HORIZONTAL,
			.destsurf = winsurf,
			.scancode = SDL_SCANCODE_RETURN,
			.center = {
				(PLACE_CHOOSER_WIDTH + CAMERA_SCREEN_WIDTH)/2,
				CAMERA_SCREEN_HEIGHT - PLACE_CHOOSER_HEIGHT/2,
			},
			.onclick = on_play_button_clicked,
			// onclickdata is set in chooser_run()
		},
		.placech = {
			.pl = &place_list()[0],
			.cam = {
				.screencentery = -0.55f*PLACE_CHOOSER_HEIGHT,
				.surface = misc_create_cropped_surface(winsurf, (SDL_Rect){
					0, CAMERA_SCREEN_HEIGHT - PLACE_CHOOSER_HEIGHT + BUTTON_SIZE_SMALL,
					PLACE_CHOOSER_WIDTH, PLACE_CHOOSER_HEIGHT - 2*BUTTON_SIZE_SMALL
				}),
				.angle = 0,
			},
			.prevbtn = {
				.imgpath = "arrows/up.png",
				.flags = BUTTON_HORIZONTAL,
				.destsurf = winsurf,
				// TODO: two scancodes, 'w' and ↑
				.center = {
					PLACE_CHOOSER_WIDTH/2,
					CAMERA_SCREEN_HEIGHT - PLACE_CHOOSER_HEIGHT + BUTTON_SIZE_SMALL/2,
				},
				.onclick = select_prev_place,
			},
			.nextbtn = {
				.imgpath = "arrows/down.png",
				.flags = BUTTON_HORIZONTAL,
				.destsurf = winsurf,
				// TODO: two scancodes, 's' and ↓
				.center = {
					PLACE_CHOOSER_WIDTH/2,
					CAMERA_SCREEN_HEIGHT - BUTTON_SIZE_SMALL/2,
				},
				.onclick = select_next_place,
			},
		},
	};
	ch->placech.prevbtn.onclickdata = &ch->placech;
	ch->placech.nextbtn.onclickdata = &ch->placech;

	create_player_ellipsoids(ch->ellipsoids, ch->winsurf->format);
	setup_player_chooser(ch, 0, SDL_SCANCODE_A, SDL_SCANCODE_D);
	setup_player_chooser(ch, 1, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT);
}

void chooser_destroy(const struct Chooser *ch)
{
	SDL_FreeSurface(ch->playerch[0].cam.surface);
	SDL_FreeSurface(ch->playerch[1].cam.surface);
	SDL_FreeSurface(ch->placech.cam.surface);
}

static void show_choose_player_text(SDL_Surface *winsurf)
{
	SDL_Surface *s = misc_create_text_surface("Choose players and place:", white_color, FONT_SIZE);
	misc_blit_with_center(s, winsurf, &(SDL_Point){ winsurf->w/2, FONT_SIZE/2 });
	SDL_FreeSurface(s);
}

enum MiscState chooser_run(struct Chooser *ch)
{
	bool playbtnclicked = false;
	ch->bigplaybtn.onclickdata = &playbtnclicked;

	SDL_FillRect(ch->winsurf, NULL, 0);
	button_show(&ch->bigplaybtn);
	button_show(&ch->placech.prevbtn);
	button_show(&ch->placech.nextbtn);
	show_player_chooser_in_beginning(&ch->playerch[0]);
	show_player_chooser_in_beginning(&ch->playerch[1]);
	show_choose_player_text(ch->winsurf);

	struct LoopTimer lt = {0};

	while(!playbtnclicked) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (!handle_event(&e, ch))
				return MISC_STATE_QUIT;
		}

		for (int i = 0; i < FILELIST_NPLAYERS; i++) {
			ch->ellipsoids[i].angle += 1.0f / CAMERA_FPS;
			ellipsoid_update_transforms(&ch->ellipsoids[i]);
		}

		turn_camera(&ch->playerch[0]);
		turn_camera(&ch->playerch[1]);
		show_player_chooser_each_frame(ch, &ch->playerch[0]);
		show_player_chooser_each_frame(ch, &ch->playerch[1]);

		show_the_place(&ch->placech);

		SDL_UpdateWindowSurface(ch->win);
		looptimer_wait(&lt);
	}
	return MISC_STATE_PLAY;
}
