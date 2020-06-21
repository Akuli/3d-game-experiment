#include "chooser.h"
#include "../generated/filelist.h"
#include "button.h"
#include "camera.h"
#include "ellipsoid.h"
#include "looptimer.h"
#include "mathstuff.h"
#include "misc.h"
#include "place.h"
#include "player.h"
#include "showall.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define FONT_SIZE 40

#define PLAYER_CHOOSER_HEIGHT ( 0.4f*CAMERA_SCREEN_HEIGHT )
#define PLACE_CHOOSER_HEIGHT (CAMERA_SCREEN_HEIGHT - PLAYER_CHOOSER_HEIGHT)

#define ELLIPSOID_XZ_DISTANCE_FROM_ORIGIN 2.0f
#define CAMERA_XZ_DISTANCE_FROM_ORIGIN 5.0f
#define CAMERA_Y 1.6f

static const SDL_Color white_color = { 0xff, 0xff, 0xff, 0xff };

static void calculate_player_chooser_geometry_stuff(
	int leftx, SDL_Rect *preview, SDL_Point *prevbcenter, SDL_Point *nextbcenter)
{
	preview->w = CAMERA_SCREEN_WIDTH/2 - 2*BUTTON_SIZE_SMALL;
	preview->h = PLAYER_CHOOSER_HEIGHT;
	preview->x = leftx + CAMERA_SCREEN_WIDTH/4 - preview->w/2;
	preview->y = FONT_SIZE;

	prevbcenter->x = leftx + BUTTON_SIZE_SMALL/2;
	nextbcenter->x = leftx + CAMERA_SCREEN_WIDTH/2 - BUTTON_SIZE_SMALL/2;
	prevbcenter->y = FONT_SIZE + PLAYER_CHOOSER_HEIGHT/2;
	nextbcenter->y = FONT_SIZE + PLAYER_CHOOSER_HEIGHT/2;
}

static void update_player_name_display(struct ChooserPlayerStuff *plrch)
{
	if (plrch->nametextsurf)
		SDL_FreeSurface(plrch->nametextsurf);

	char name[100];
	player_epic_name(plrch->epic, name, sizeof name);
	plrch->nametextsurf = misc_create_text_surface(name, white_color, FONT_SIZE);
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
			.screencentery = 0,
			.surface = misc_create_cropped_surface(ch->winsurf, preview),
			.angle = -(2*pi)/FILELIST_NPLAYERS * idx,
		},
	};

	update_player_name_display(plrch);
	button_refresh(&plrch->prevbtn);
	button_refresh(&plrch->nextbtn);
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
	button_handle_event(evt, &ch->bigplaybtn);
	return true;
}

static void show_player_chooser(const struct Chooser *ch, const struct ChooserPlayerStuff *plrch)
{
	show_all(NULL, 0, ch->ellipsoids, FILELIST_NPLAYERS, &plrch->cam);
	button_show(&plrch->prevbtn);
	button_show(&plrch->nextbtn);
	misc_blit_with_center(plrch->nametextsurf, ch->winsurf, &(SDL_Point){
		plrch->leftx + ch->winsurf->w/4,
		FONT_SIZE + PLAYER_CHOOSER_HEIGHT + FONT_SIZE/2,
	});
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
			.big = true,
			.horizontal = true,
			.destsurf = winsurf,
			.scancode = SDL_SCANCODE_RETURN,
			.center = { CAMERA_SCREEN_WIDTH/2, (CAMERA_SCREEN_HEIGHT + PLAYER_CHOOSER_HEIGHT)/2 },
			.onclick = on_play_button_clicked,
			// onclickdata is set in chooser_run()
		},
		.chooseplayertext = misc_create_text_surface("Choose players:", white_color, FONT_SIZE),
	};

	button_refresh(&ch->bigplaybtn);
	create_player_ellipsoids(ch->ellipsoids, ch->winsurf->format);
	setup_player_chooser(ch, 0, SDL_SCANCODE_A, SDL_SCANCODE_D);
	setup_player_chooser(ch, 1, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT);
}

void chooser_destroy(const struct Chooser *ch)
{
	for (int i = 0; i < 2; i++) {
		SDL_FreeSurface(ch->playerch[i].cam.surface);
		button_destroy(&ch->playerch[i].prevbtn);
		button_destroy(&ch->playerch[i].nextbtn);
	}
	button_destroy(&ch->bigplaybtn);
	SDL_FreeSurface(ch->chooseplayertext);
}

enum MiscState chooser_run(struct Chooser *ch)
{
	bool playbtnclicked = false;
	ch->bigplaybtn.onclickdata = &playbtnclicked;

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

		SDL_FillRect(ch->winsurf, NULL, 0);
		misc_blit_with_center(ch->chooseplayertext, ch->winsurf,
			&(SDL_Point){ch->winsurf->w/2, FONT_SIZE/2});
		show_player_chooser(ch, &ch->playerch[0]);
		show_player_chooser(ch, &ch->playerch[1]);
		button_show(&ch->bigplaybtn);

		SDL_UpdateWindowSurface(ch->win);
		looptimer_wait(&lt);
	}
	return MISC_STATE_PLAY;
}
