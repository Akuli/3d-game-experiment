#include "chooser.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
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

#define ELLIPSOID_XZ_DISTANCE_FROM_ORIGIN 2.0f
#define CAMERA_XZ_DISTANCE_FROM_ORIGIN 5.0f
#define CAMERA_Y 1.6f

static const SDL_Color white_color = { 0xff, 0xff, 0xff, 0xff };

static void update_player_name_display(struct ChooserPlayerStuff *plrch)
{
	SDL_assert(plrch->prevbtn.destsurf == plrch->nextbtn.destsurf);
	SDL_Surface *winsurf = plrch->nextbtn.destsurf;
	SDL_Point center = {
		plrch->leftx + CAMERA_SCREEN_WIDTH/4,
		PLAYER_CHOOSER_HEIGHT - FONT_SIZE/2 - 5,  // -5 because apparently text can go below bottom
	};

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

static void rotate_left (void *ch) { rotate_player_chooser(ch, -1); }
static void rotate_right(void *ch) { rotate_player_chooser(ch, +1); }

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

static void turn_camera(struct ChooserPlayerStuff *plrch)
{
	float maxturn = 50.0f / (CAMERA_FPS * player_nepics);
	float turn = plrch->anglediff;
	clamp_float(&turn, -maxturn, +maxturn);
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

static void on_copy_clicked(void *chptr)
{
	struct Chooser *ch = chptr;
	ch->mapch.listbox.selectidx = map_copy(&ch->mapch.maps, &ch->mapch.nmaps, ch->mapch.listbox.selectidx);
	mapeditor_setmap(ch->editor, &ch->mapch.maps[ch->mapch.listbox.selectidx]);
	ch->mapch.listbox.redraw = true;
}
static void on_edit_clicked(void *chptr) { ((struct Chooser *)chptr)->state = MISC_STATE_MAPEDITOR; }
static void on_delete_clicked(void *chptr) { ((struct Chooser *)chptr)->state = MISC_STATE_DELETEMAP; }

static const struct ListboxEntry *get_listbox_entry(void *chptr, int i)
{
	struct Chooser *ch = chptr;
	if (i < 0 || i >= ch->mapch.nmaps)
		return NULL;

	static struct ListboxEntry res;
	res = (struct ListboxEntry){
		.text = ch->mapch.maps[i].name,
		.buttons = {
			{
				.text = ch->mapch.maps[i].custom ? "Edit" : NULL,  // disable for non-custom maps
				.scancodes = { SDL_SCANCODE_E },
				.onclick = on_edit_clicked,
				.onclickdata = ch,
			},
			{
				.text = ch->mapch.maps[i].custom ? "Delete" : NULL,  // disable for non-custom maps
				.scancodes = { SDL_SCANCODE_DELETE },
				.onclick = on_delete_clicked,
				.onclickdata = ch,
			},
			{
				.text = "Copy",
				.scancodes = { SDL_SCANCODE_C },
				.onclick = on_copy_clicked,
				.onclickdata = ch,
			},
		},
		.movable = ch->mapch.maps[i].custom,
	};
	return &res;
}

static void move_map(void *chptr, int from, int to)
{
	const struct Chooser *ch = chptr;
	SDL_assert(ch->mapch.maps[from].custom);
	log_printf("Moving map \"%s\" from index %d to index %d", ch->mapch.maps[from].name, from, to);

	static struct Map tmp;  // static to keep stack usage down
	tmp = ch->mapch.maps[from];

	SDL_assert(from != to);
	if (from < to)
		memmove(&ch->mapch.maps[from], &ch->mapch.maps[from+1], (to-from)*sizeof(ch->mapch.maps[0]));
	else
		memmove(&ch->mapch.maps[to+1], &ch->mapch.maps[to], (from-to)*sizeof(ch->mapch.maps[0]));

	ch->mapch.maps[to] = tmp;
	map_update_sortkey(ch->mapch.maps, ch->mapch.nmaps, to);
}

static void handle_event(const SDL_Event *evt, struct Chooser *ch)
{
	for (int i = 0; i < 2; i++) {
		button_handle_event(evt, &ch->playerch[i].prevbtn);
		button_handle_event(evt, &ch->playerch[i].nextbtn);
	}
	button_handle_event(evt, &ch->playbtn);
	button_handle_event(evt, &ch->quitbtn);

	int oldidx = ch->mapch.listbox.selectidx;
	listbox_handle_event(&ch->mapch.listbox, evt);
	if (ch->mapch.listbox.selectidx != oldidx)
		mapeditor_setmap(ch->editor, &ch->mapch.maps[ch->mapch.listbox.selectidx]);
}

static void on_play_clicked(void *ptr) { *(enum MiscState *)ptr = MISC_STATE_PLAY; }
static void on_quit_clicked(void *ptr) { *(enum MiscState *)ptr = MISC_STATE_QUIT; }

void chooser_init(struct Chooser *ch, SDL_Window *win)
{
	SDL_Surface *winsurf = SDL_GetWindowSurface(win);
	if (!winsurf)
		log_printf_abort("SDL_GetWindowSurface failed: %s", SDL_GetError());

	*ch = (struct Chooser){
		.win = win,
		.winsurf = winsurf,
		.playbtn = {
			.text = "Play",
			.destsurf = winsurf,
			.scancodes = { SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE },
			.center = {
				(LISTBOX_WIDTH + CAMERA_SCREEN_WIDTH)/2,
				CAMERA_SCREEN_HEIGHT - button_height(0)/2,
			},
			.onclick = on_play_clicked,
			.onclickdata = &ch->state,
		},
		.quitbtn = {
			.text = "Quit",
			.flags = BUTTON_TINY,
			.destsurf = winsurf,
			.scancodes = { SDL_SCANCODE_ESCAPE },
			.center = {
				CAMERA_SCREEN_WIDTH - button_width(BUTTON_TINY)/2 - 10,
				button_height(BUTTON_TINY)/2 + 10,
			},
			.onclick = on_quit_clicked,
			.onclickdata = &ch->state,
		},
		.mapch = {
			.listbox = {
				.destsurf = winsurf,
				.destrect = {
#define YMARGIN 5
					.x = 0,
					.y = PLAYER_CHOOSER_HEIGHT + YMARGIN,
					.w = LISTBOX_WIDTH,
					.h = CAMERA_SCREEN_HEIGHT - PLAYER_CHOOSER_HEIGHT - 2*YMARGIN,
#undef YMARGIN
				},
				.upscancodes = { SDL_SCANCODE_W, SDL_SCANCODE_UP },
				.downscancodes = { SDL_SCANCODE_S, SDL_SCANCODE_DOWN },
				.getentry = get_listbox_entry,
				.move = move_map,
				.cbdata = ch,
			},
		},
	};

	create_player_ellipsoids(ch);
	setup_player_chooser(ch, 0, SDL_SCANCODE_A, SDL_SCANCODE_D);
	setup_player_chooser(ch, 1, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT);

	ch->mapch.maps = map_list(&ch->mapch.nmaps);
	listbox_init(&ch->mapch.listbox);

	ch->editorsurf = misc_create_cropped_surface(winsurf, (SDL_Rect){
		.x = LISTBOX_WIDTH,
		.y = PLAYER_CHOOSER_HEIGHT,
		.w = CAMERA_SCREEN_WIDTH - LISTBOX_WIDTH,
		.h = CAMERA_SCREEN_HEIGHT - PLAYER_CHOOSER_HEIGHT - button_height(0),
	});
	ch->editor = mapeditor_new(ch->editorsurf, -0.22f*CAMERA_SCREEN_HEIGHT, 0.6f);
	mapeditor_setmap(ch->editor, &ch->mapch.maps[ch->mapch.listbox.selectidx]);
	mapeditor_setplayers(ch->editor, ch->playerch[0].epic, ch->playerch[1].epic);
}

void chooser_destroy(const struct Chooser *ch)
{
	listbox_destroy(&ch->mapch.listbox);
	SDL_FreeSurface(ch->playerch[0].cam.surface);
	SDL_FreeSurface(ch->playerch[1].cam.surface);
	SDL_FreeSurface(ch->editorsurf);
	free(ch->editor);
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
	// Maps can be deleted while chooser not running
	clamp(&ch->mapch.listbox.selectidx, 0, ch->mapch.nmaps-1);
	mapeditor_setmap(ch->editor, &ch->mapch.maps[ch->mapch.listbox.selectidx]);
	ch->mapch.listbox.redraw = true;

	SDL_FillRect(ch->winsurf, NULL, 0);
	button_show(&ch->playbtn);
	button_show(&ch->quitbtn);
	show_player_chooser_in_beginning(&ch->playerch[0]);
	show_player_chooser_in_beginning(&ch->playerch[1]);
	show_title_text(ch->winsurf);

	ch->state = MISC_STATE_CHOOSER;
	struct LoopTimer lt = {0};

	while(1) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				return MISC_STATE_QUIT;
			handle_event(&e, ch);
		}

		if (ch->state != MISC_STATE_CHOOSER)
			return ch->state;

		rotate_player_ellipsoids(ch->ellipsoids);
		show_player_chooser_each_frame(ch, &ch->playerch[0]);
		show_player_chooser_each_frame(ch, &ch->playerch[1]);
		mapeditor_setplayers(ch->editor, ch->playerch[0].epic, ch->playerch[1].epic);
		mapeditor_displayonly_eachframe(ch->editor);
		listbox_show(&ch->mapch.listbox);

		SDL_UpdateWindowSurface(ch->win);
		looptimer_wait(&lt);
	}
}
