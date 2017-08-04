/*
 * Copyright (C) 2017 Fredrik Wikstrom <fredrik@a500.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <cairo.h>
#include <cairo-amigaos.h>
#include <stdlib.h>

typedef struct _cairo_amigaos_window {
	struct Window   *window;
	struct BitMap   *bitmap;
	int              width, height;
	cairo_surface_t *surface;
	cairo_t         *cairo;
} cairo_amigaos_window_t;

static void
draw_rectangles(cairo_amigaos_window_t *cairo_win, int num)
{
	double red, green, blue;
	double x, y, w, h;
	int    i;

	for (i = 0; i < num; i++) {
		red   = (rand() % 256) / 255.0;
		green = (rand() % 256) / 255.0;
		blue  = (rand() % 256) / 255.0;

		cairo_set_source_rgb(cairo_win->cairo, red, green, blue);

		x = rand() % cairo_win->width;
		y = rand() % cairo_win->height;
		w = rand() % (cairo_win->width / 2);
		h = rand() % (cairo_win->height / 2);

		cairo_rectangle(cairo_win->cairo, x, y, w, h);

		cairo_fill(cairo_win->cairo);
	}
}

static void
refresh_window(cairo_amigaos_window_t *cairo_win)
{
	struct Window *window = cairo_win->window;

	IGraphics->BltBitMapTags(
		BLITA_SrcType,  BLITT_BITMAP,
		BLITA_Source,   cairo_win->bitmap,
		BLITA_DestType, BLITT_RASTPORT,
		BLITA_Dest,     window->RPort,
		BLITA_Width,    cairo_win->width,
		BLITA_Height,   cairo_win->height,
		BLITA_DestX,    window->BorderLeft,
		BLITA_DestY,    window->BorderTop,
		TAG_END);
}

static void
destroy_window(cairo_amigaos_window_t *cairo_win)
{
	if (cairo_win == NULL)
		return;

	IIntuition->CloseWindow(cairo_win->window);
	cairo_destroy(cairo_win->cairo);
	cairo_surface_destroy(cairo_win->surface);
	IGraphics->FreeBitMap(cairo_win->bitmap);
	free(cairo_win);
}

static cairo_amigaos_window_t *
create_window(int width,
              int height)
{
	cairo_amigaos_window_t *cairo_win;
	struct Screen          *screen;
	int                     window_left, window_top;
	int                     window_width, window_height;

	screen = IIntuition->LockPubScreen(NULL);
	if (screen == NULL)
		return NULL;

	cairo_win = malloc(sizeof(cairo_amigaos_window_t));

	cairo_win->width  = width;
	cairo_win->height = height;

	/* Create as friend bitmap of screen */
	cairo_win->bitmap = IGraphics->AllocBitMapTags(width, height, 24,
		BMATags_Friend, screen->RastPort.BitMap,
		TAG_END);
	if (cairo_win->bitmap == NULL) {
		IIntuition->UnlockPubScreen(NULL, screen);
		free(cairo_win);
		return NULL;
	}

	cairo_win->surface = cairo_amigaos_surface_create(cairo_win->bitmap);
	if (cairo_win->surface == NULL) {
		IIntuition->UnlockPubScreen(NULL, screen);
		IGraphics->FreeBitMap(cairo_win->bitmap);
		free(cairo_win);
		return NULL;
	}

	cairo_win->cairo = cairo_create(cairo_win->surface);
	if (cairo_win->cairo == NULL) {
		IIntuition->UnlockPubScreen(NULL, screen);
		cairo_surface_destroy(cairo_win->surface);
		IGraphics->FreeBitMap(cairo_win->bitmap);
		free(cairo_win);
		return NULL;
	}

	cairo_set_source_rgb(cairo_win->cairo, 0.0, 0.0, 0.0);
	cairo_paint(cairo_win->cairo);

	/* Center window in screen */
	window_width  = width + screen->WBorLeft + screen->WBorRight;
	window_height = height + screen->WBorTop + screen->Font->ta_YSize + 1 + screen->WBorBottom;
	window_left   = (screen->Width - window_width) >> 1;
	window_top    = (screen->Height - window_height) >> 1;

	cairo_win->window = IIntuition->OpenWindowTags(NULL,
		WA_PubScreen,   screen,
		WA_Title,       "rectangles",
		WA_Flags,       WFLG_CLOSEGADGET|WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_ACTIVATE,
		WA_IDCMP,       IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW|IDCMP_INTUITICKS,
		WA_Left,        window_left,
		WA_Top,         window_top,
		WA_Width,       window_width,
		WA_Height,      window_height,
		TAG_END);
	IIntuition->UnlockPubScreen(NULL, screen);
	if (cairo_win->window == NULL) {
		cairo_destroy(cairo_win->cairo);
		cairo_surface_destroy(cairo_win->surface);
		IGraphics->FreeBitMap(cairo_win->bitmap);
		free(cairo_win);
		return NULL;
	}

	refresh_window(cairo_win);

	return cairo_win;
}

int main(void) {
	cairo_amigaos_window_t *cairo_win;
	struct Window          *window;
	struct IntuiMessage    *imsg;
	BOOL                    done = FALSE;

	/* Seed the random number generator */
	srand(0xDEADBEEF);

	cairo_win = create_window(640, 480);
	if (cairo_win == NULL)
		return RETURN_ERROR;

	window = cairo_win->window;

	while (!done) {
		IExec->WaitPort(window->UserPort);

		while ((imsg = (struct IntuiMessage *)IExec->GetMsg(window->UserPort)) != NULL) {
			switch (imsg->Class) {

				case IDCMP_CLOSEWINDOW:
					done = TRUE;
					break;

				case IDCMP_REFRESHWINDOW:
					/* Needed for simple refresh windows */
					IIntuition->BeginRefresh(window);
					refresh_window(cairo_win);
					IIntuition->EndRefresh(window, TRUE);
					break;

				case IDCMP_INTUITICKS:
					draw_rectangles(cairo_win, 1);
					refresh_window(cairo_win);
					break;

			}

			IExec->ReplyMsg((struct Message *)imsg);
		}
	}

	destroy_window(cairo_win);

	return RETURN_OK;
}

