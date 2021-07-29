#include "textentry.h"
#include <stdbool.h>
#include <limits.h>
#include "log.h"
#include "misc.h"
#include "mathstuff.h"

static int text_width(const struct TextEntry *te, const char *s)
{
	int w, h;
	if (TTF_SizeUTF8(te->font, s, &w, &h) < 0)
		log_printf_abort("TTF_SizeUTF8 failed: %s", TTF_GetError());
	return w;
}

static char *mouse_to_cursorpos(const struct TextEntry *te, int mousex)
{
	mousex = mousex - te->rect.x - te->rect.w/2 + text_width(te, te->text)/2;

	char *left = malloc(strlen(te->text) + 1);
	if (!left)
		log_printf_abort("out of mem");
	strcpy(left, te->text);

	// Dumb and working algorithm. Let data be smol.
	int mindist = INT_MAX;
	int cur = 0;
	for (int i = strlen(te->text); i >= 0; i--) {
		left[i] = '\0';
		int d = abs(text_width(te, left) - mousex);
		if (d < mindist) {
			mindist = d;
			cur = i;
		}
	}

	free(left);
	return te->text + cur;
}

// FIXME: ääkköset
static void utf8_prev(char **s) { --*s; }
static void utf8_next(char **s) { ++*s; }


bool textentry_handle_event(struct TextEntry *te, const SDL_Event *e)
{
	if (e->type != SDL_MOUSEBUTTONDOWN && !te->cursor)
		return false;

	switch(e->type) {
	case SDL_MOUSEBUTTONDOWN:
		if (SDL_PointInRect(&(SDL_Point){ e->button.x, e->button.y }, &te->rect)) {
			te->cursor = mouse_to_cursorpos(te, e->button.x);
			te->blinkstart = SDL_GetTicks();
		} else {
			te->cursor = NULL;
		}
		textentry_show(te);
		return false;

	case SDL_KEYDOWN:
		switch(misc_handle_scancode(e->key.keysym.scancode)) {
		case SDL_SCANCODE_LEFT:
			if (te->cursor > te->text)
				utf8_prev(&te->cursor);
			break;
		case SDL_SCANCODE_RIGHT:
			if (*te->cursor)
				utf8_next(&te->cursor);
			break;
		case SDL_SCANCODE_BACKSPACE:
			if (te->cursor > te->text) {
				char *end = te->cursor;
				utf8_prev(&te->cursor);
				memmove(te->cursor, end, strlen(end)+1);
			}
			break;
		case SDL_SCANCODE_DELETE:
			if (*te->cursor) {
				char *end = te->cursor;
				utf8_next(&end);
				memmove(te->cursor, end, strlen(end)+1);
			}
			break;
		case SDL_SCANCODE_HOME:
			te->cursor = te->text;
			break;
		case SDL_SCANCODE_END:
			te->cursor += strlen(te->cursor);
			break;
		default:
			return false;
		}

		te->blinkstart = SDL_GetTicks();
		return true;

	case SDL_TEXTINPUT:
	{
		char *add = malloc(strlen(e->text.text) + 1);
		if (!add)
			log_printf_abort("out of mem");

		// No idea why sdl2 adds many 1 bytes, deleting
		strcpy(add, e->text.text);
		char *p;
		while ((p = strchr(add, 1)))
			memmove(p, p+1, strlen(p+1)+1);

		if (strlen(te->text) + strlen(add) > te->maxlen)
			return false;

		memmove(te->cursor + strlen(add), te->cursor, strlen(te->cursor) + 1);
		memcpy(te->cursor, add, strlen(add));
		te->cursor += strlen(add);
		free(add);
		return true;
	}

	default:
		return false;
	}
}

void textentry_show(struct TextEntry *te)
{
	uint32_t now = SDL_GetTicks();
	int oldblink = ((te->lastredraw - te->blinkstart) / 500) % 2;
	int newblink = ((now - te->blinkstart) / 500) % 2;
	if (oldblink == newblink && !te->redraw)
		return;

	te->redraw = false;
	te->lastredraw = now;

	SDL_FillRect(te->surf, &te->rect, 0);

	// font not cached because this isn't perf critical
	// TODO: same font loading code is in misc.c
	if (!te->font) {
		te->font = TTF_OpenFont("assets/DejaVuSans.ttf", te->fontsz);
		if (!te->font)
			log_printf_abort("TTF_OpenFont failed: %s", TTF_GetError());
	}

	SDL_Color white = {0xff,0xff,0xff,0xff};
	uint32_t white2 = SDL_MapRGBA(te->surf->format, 0xff,0xff,0xff,0xff);

	SDL_Point center = { te->rect.x + te->rect.w/2, te->rect.y + te->rect.h/2 };

	// it errors for empty text, lol
	if (*te->text) {
		SDL_Surface *s = TTF_RenderUTF8_Blended(te->font, te->text, white);
		if (!s)
			log_printf_abort("TTF_RenderUTF8_Blended failed with text \"%s\": %s", te->text, TTF_GetError());

		// Handle text too long to fit, without writing beyond end of surface
		SDL_Rect wantsrc = { 0, 0, s->w, s->h };
		SDL_Rect fitsrc = { s->w/2 - te->rect.w/2, s->h/2 - te->rect.h/2, te->rect.w, te->rect.h };
		SDL_Rect smolsrc;
		SDL_IntersectRect(&wantsrc, &fitsrc, &smolsrc);
		SDL_BlitSurface(s, &smolsrc, te->surf, &(SDL_Rect){ center.x - smolsrc.w/2, center.y - smolsrc.h/2 });
		SDL_FreeSurface(s);
	}

	if (te->cursor != NULL && newblink == 0) {
		char *left = malloc(te->cursor - te->text + 1);
		if (!left)
			log_printf_abort("out of mem");
		memcpy(left, te->text, te->cursor - te->text);
		left[te->cursor - te->text] = '\0';

		int leftw, lefth;
		int fullw, fullh;
		if (TTF_SizeUTF8(te->font, left, &leftw, &lefth) < 0 || TTF_SizeUTF8(te->font, te->text, &fullw, &fullh) < 0)
			log_printf_abort("TTF_SizeUTF8 failed: %s", TTF_GetError());
		free(left);

		int x = center.x - fullw/2 + leftw;
		SDL_Rect r = {x-1, center.y - te->fontsz/2, 3, te->fontsz};
		SDL_Rect clipped;
		SDL_IntersectRect(&r, &te->rect, &clipped);
		SDL_FillRect(te->surf, &clipped, white2);
	}
}
