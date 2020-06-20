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

static const SDL_Color white_color = { 0xff, 0xff, 0xff, 0xff };

struct PlayerChooser {
	int leftx;
	const struct EllipsoidPic *epic;   // pointer into player_getepics()
	float anglediff;    // how much the player chooser is about to spin
	struct Button prevbtn, nextbtn;
	struct Camera cam;
	SDL_Surface *nametextsurf;
};

struct ChooserState {
	SDL_Surface *winsurf;
	struct PlayerChooser playerch[2];
	struct Ellipsoid ellipsoids[FILELIST_NPLAYERS];
	struct Button bigplaybtn;
};

static void calculate_player_chooser_geometry_stuff(
	int leftx, SDL_Rect *preview, SDL_Point *prevbcenter, SDL_Point *nextbcenter)
{
	preview->w = CAMERA_SCREEN_WIDTH/2 - 2*SMALL_BUTTON_SIZE;
	preview->h = PLAYER_CHOOSER_HEIGHT;
	preview->x = leftx + CAMERA_SCREEN_WIDTH/4 - preview->w/2;
	preview->y = FONT_SIZE;

	prevbcenter->x = leftx + SMALL_BUTTON_SIZE/2;
	nextbcenter->x = leftx + CAMERA_SCREEN_WIDTH/2 - SMALL_BUTTON_SIZE/2;
	prevbcenter->y = FONT_SIZE + PLAYER_CHOOSER_HEIGHT/2;
	nextbcenter->y = FONT_SIZE + PLAYER_CHOOSER_HEIGHT/2;
}

static void update_name_display(struct PlayerChooser *ch)
{
	if (ch->nametextsurf)
		SDL_FreeSurface(ch->nametextsurf);
	ch->nametextsurf = misc_create_text_surface(
		player_getname(ch->epic), white_color, FONT_SIZE);
}

static void rotate_player_chooser(struct PlayerChooser *ch, int dir)
{
	assert(dir == +1 || dir == -1);

	ch->epic += dir;
	if (ch->epic == player_get_epics(NULL) - 1)
		ch->epic += FILELIST_NPLAYERS;
	else if (ch->epic == player_get_epics(NULL) + FILELIST_NPLAYERS)
		ch->epic -= FILELIST_NPLAYERS;
	update_name_display(ch);

	// why subtracting: more angle = clockwise from above = left in chooser
	float pi = acosf(-1);
	ch->anglediff -= dir * (2*pi) / (float)FILELIST_NPLAYERS;
}

static void rotate_left (void *playerch) { rotate_player_chooser(playerch, -1); }
static void rotate_right(void *playerch) { rotate_player_chooser(playerch, +1); }

static void setup_player_chooser(struct ChooserState *st, int idx, int scprev, int scnext)
{
	int leftx = idx * (st->winsurf->w/2);

	SDL_Rect preview;
	SDL_Point prevc, nextc;
	calculate_player_chooser_geometry_stuff(leftx, &preview, &prevc, &nextc);

	float pi = acosf(-1);
	struct PlayerChooser *ch = &st->playerch[idx];
	*ch = (struct PlayerChooser){
		.epic = st->ellipsoids[idx].epic,
		.leftx = leftx,
		.prevbtn = {
			.imgpath = "arrows/left.png",
			.scancode = scprev,
			.destsurf = st->winsurf,
			.center = prevc,
			.onclick = rotate_left,
		},
		.nextbtn = {
			.imgpath = "arrows/right.png",
			.scancode = scnext,
			.destsurf = st->winsurf,
			.center = nextc,
			.onclick = rotate_right,
		},
		.cam = {
			.screencentery = 0,
			.surface = misc_create_cropped_surface(st->winsurf, preview),
			.angle = -(2*pi)/FILELIST_NPLAYERS * idx,
		},
	};

	ch->prevbtn.onclickdata = ch;
	ch->nextbtn.onclickdata = ch;

	update_name_display(ch);
	button_refresh(&ch->prevbtn);
	button_refresh(&ch->nextbtn);
	camera_update_caches(&ch->cam);
}

static void turn_camera(struct PlayerChooser *ch)
{
	float maxturn = 50.0f / (CAMERA_FPS * FILELIST_NPLAYERS);
	assert(maxturn > 0);
	float turn = max(-maxturn, min(maxturn, ch->anglediff));

	ch->cam.angle += turn;
	ch->anglediff -= turn;

	ch->cam.location = mat3_mul_vec3(mat3_rotation_xz(ch->cam.angle), (Vec3){0,0.7f,3.0f});
	camera_update_caches(&ch->cam);
}

// returns false when game should quit
static bool handle_event(const SDL_Event *evt, struct ChooserState *st)
{
	if (evt->type == SDL_QUIT)
		return false;

	for (int i = 0; i < 2; i++) {
		button_handle_event(evt, &st->playerch[i].prevbtn);
		button_handle_event(evt, &st->playerch[i].nextbtn);
	}
	button_handle_event(evt, &st->bigplaybtn);
	return true;
}

static void show_player_chooser(const struct ChooserState *st, const struct PlayerChooser *ch)
{
	show_all(NULL, 0, st->ellipsoids, FILELIST_NPLAYERS, &ch->cam);
	button_show(&ch->prevbtn);
	button_show(&ch->nextbtn);
	misc_blit_with_center(ch->nametextsurf, st->winsurf, &(SDL_Point){
		ch->leftx + st->winsurf->w/4,
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
			.center = mat3_mul_vec3(mat3_rotation_xz(angle), (Vec3){ 1.4f, 0, 0 }),
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

enum MiscState chooser_run(
	SDL_Window *win,
	const struct EllipsoidPic **plr1epic, const struct EllipsoidPic **plr2epic,
	const struct Place **pl)
{
	bool playbtnclicked = false;
	struct ChooserState st = {
		.winsurf = SDL_GetWindowSurface(win),
		.bigplaybtn = {
			.text = "Play",
			.big = true,
			.horizontal = true,
			.scancode = SDL_SCANCODE_RETURN,
			.destsurf = st.winsurf,
			.center = { CAMERA_SCREEN_WIDTH/2, (CAMERA_SCREEN_HEIGHT + PLAYER_CHOOSER_HEIGHT)/2 },
			.onclick = on_play_button_clicked,
			.onclickdata = &playbtnclicked,
		},
	};

	if (!st.winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());
	button_refresh(&st.bigplaybtn);

	// TODO: implement a place chooser
	*pl = &place_list()[0];

	create_player_ellipsoids(st.ellipsoids, st.winsurf->format);
	setup_player_chooser(&st, 0, SDL_SCANCODE_A, SDL_SCANCODE_D);
	setup_player_chooser(&st, 1, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT);

	SDL_Surface *chooseplrtxt = misc_create_text_surface(
		"Choose players:", white_color, FONT_SIZE);

	struct LoopTimer lt = {0};

	while(!playbtnclicked) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (!handle_event(&e, &st))
				goto out;
		}

		for (int i = 0; i < FILELIST_NPLAYERS; i++) {
			st.ellipsoids[i].angle += 1.0f / CAMERA_FPS;
			ellipsoid_update_transforms(&st.ellipsoids[i]);
		}

		turn_camera(&st.playerch[0]);
		turn_camera(&st.playerch[1]);

		SDL_FillRect(st.winsurf, NULL, 0);
		misc_blit_with_center(chooseplrtxt, st.winsurf, &(SDL_Point){st.winsurf->w/2, FONT_SIZE/2});
		show_player_chooser(&st, &st.playerch[0]);
		show_player_chooser(&st, &st.playerch[1]);
		button_show(&st.bigplaybtn);

		SDL_UpdateWindowSurface(win);
		looptimer_wait(&lt);
	}

out:
	for (int i = 0; i < 2; i++) {
		SDL_FreeSurface(st.playerch[i].cam.surface);
		SDL_FreeSurface(st.playerch[i].prevbtn.cachesurf);
		SDL_FreeSurface(st.playerch[i].nextbtn.cachesurf);
	}

	*plr1epic = st.playerch[0].epic;
	*plr2epic = st.playerch[1].epic;
	return playbtnclicked ? MISC_STATE_PLAY : MISC_STATE_QUIT;
}
