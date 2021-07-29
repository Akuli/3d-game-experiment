#include "textentry.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <SDL2/SDL_ttf.h>
#include "log.h"
#include "misc.h"

static int text_width(const struct TextEntry *te, const char *s)
{
	int w, h;
	if (TTF_SizeUTF8(misc_get_font(te->fontsz), s, &w, &h) < 0)
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

// https://en.wikipedia.org/wiki/UTF-8#Encoding
#define byte10xxxxxx(b) ((unsigned char)(b) >> 6 == 2)
static void utf8_prev(char **s) {
	do { --*s; } while (byte10xxxxxx(**s));
}
static void utf8_next(char **s) {
	do { ++*s; } while (byte10xxxxxx(**s));
}


void textentry_handle_event(struct TextEntry *te, const SDL_Event *e)
{
	if ((
		e->type == SDL_MOUSEBUTTONDOWN &&
		SDL_PointInRect(&(SDL_Point){ e->button.x, e->button.y }, &te->rect)
	) || (
		e->type == SDL_KEYDOWN && e->key.keysym.scancode == SDL_SCANCODE_F2
	))
	{
		te->cursor = mouse_to_cursorpos(te, e->button.x);
		te->blinkstart = SDL_GetTicks();
		te->redraw = true;
		return;
	}

	if (!te->cursor)
		return;

	switch(e->type) {
	case SDL_MOUSEBUTTONDOWN:
		te->cursor = NULL;
		te->redraw = true;
		return;

	case SDL_KEYDOWN:
	{
		bool changed = false;
		bool lcontrol = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL];
		bool rcontrol = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_RCTRL];

		switch(misc_handle_scancode(e->key.keysym.scancode)) {
		case SDL_SCANCODE_LEFT:
		{
			if (te->cursor > te->text)
				utf8_prev(&te->cursor);
			if (lcontrol || rcontrol) {
				// cursor[-1] works because utf8 continuation byte can't be ascii space
				while (te->cursor > te->text && te->cursor[-1] != ' ')
					utf8_prev(&te->cursor);
			}
			break;
		}
		case SDL_SCANCODE_RIGHT:
		{
			if (*te->cursor)
				utf8_next(&te->cursor);
			if (lcontrol || rcontrol) {
				while (*te->cursor && te->cursor[-1] != ' ')
					utf8_next(&te->cursor);
			}
			break;
		}
		case SDL_SCANCODE_BACKSPACE:
			if (te->cursor > te->text) {
				char *end = te->cursor;
				utf8_prev(&te->cursor);
				memmove(te->cursor, end, strlen(end)+1);
				changed = true;
			}
			break;
		case SDL_SCANCODE_DELETE:
			if (*te->cursor) {
				char *end = te->cursor;
				utf8_next(&end);
				memmove(te->cursor, end, strlen(end)+1);
				changed = true;
			}
			break;
		case SDL_SCANCODE_HOME:
			te->cursor = te->text;
			break;
		case SDL_SCANCODE_END:
			te->cursor += strlen(te->cursor);
			break;
		case SDL_SCANCODE_RETURN:
		case SDL_SCANCODE_ESCAPE:
			te->cursor = NULL;
			te->redraw = true;
			return;
		default:
			return;
		}

		te->blinkstart = SDL_GetTicks();
		if (changed)
			te->changecb(te->changecbdata);
		return;
	}

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

		if (strlen(te->text) + strlen(add) <= te->maxlen) {
			memmove(te->cursor + strlen(add), te->cursor, strlen(te->cursor) + 1);
			memcpy(te->cursor, add, strlen(add));
			te->cursor += strlen(add);
			te->changecb(te->changecbdata);
		}

		free(add);
		return;
	}

	default:
		return;
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

	SDL_Color white = {0xff,0xff,0xff,0xff};
	uint32_t white2 = SDL_MapRGBA(te->surf->format, 0xff,0xff,0xff,0xff);

	SDL_Point center = { te->rect.x + te->rect.w/2, te->rect.y + te->rect.h/2 };

	// it errors for empty text, lol
	if (*te->text) {
		SDL_Surface *s = TTF_RenderUTF8_Blended(misc_get_font(te->fontsz), te->text, white);
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
		int fullw = text_width(te, te->text);

		char *left = malloc(te->cursor - te->text + 1);
		if (!left)
			log_printf_abort("out of mem");
		memcpy(left, te->text, te->cursor - te->text);
		left[te->cursor - te->text] = '\0';
		int leftw = text_width(te, left);
		free(left);

		int x = center.x - fullw/2 + leftw;
		SDL_Rect r = {x-1, center.y - te->fontsz/2, 3, te->fontsz};
		SDL_Rect clipped;
		SDL_IntersectRect(&r, &te->rect, &clipped);
		SDL_FillRect(te->surf, &clipped, white2);
	}
}
