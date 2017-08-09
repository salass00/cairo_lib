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

#include "support.h"
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <cairo.h>
#include <cairo-amigaos.h>
#include <stdlib.h>

static void
draw_text(cairo_amigaos_window_t *cairo_win)
{
	cairo_font_face_t *face;
	cairo_font_options_t *font_options;

	face = cairo_amigaos_font_face_create("DejaVu Sans.font");

	cairo_set_font_face(cairo_win->cairo, face);
	cairo_set_font_size(cairo_win->cairo, 48);

	font_options = cairo_font_options_create();
	cairo_font_options_set_antialias(font_options, CAIRO_ANTIALIAS_GRAY);
	cairo_set_font_options(cairo_win->cairo, font_options);

	cairo_set_source_rgb(cairo_win->cairo, 1.0, 0.0, 0.0);
	cairo_move_to(cairo_win->cairo, 40, (cairo_win->height >> 1));
	cairo_show_text(cairo_win->cairo, "Hello Amiga World!");

	cairo_set_source_rgb(cairo_win->cairo, 0.0, 1.0, 0.0);
	cairo_move_to(cairo_win->cairo, 40 + 10, (cairo_win->height >> 1) + 10);
	cairo_show_text(cairo_win->cairo, "Hello Amiga World!");

	cairo_set_source_rgb(cairo_win->cairo, 0.0, 0.0, 1.0);
	cairo_move_to(cairo_win->cairo, 40 + 20, (cairo_win->height >> 1) + 20);
	cairo_show_text(cairo_win->cairo, "Hello Amiga World!");
}

int main(void) {
	cairo_amigaos_window_t *cairo_win;
	struct Window          *window;
	struct IntuiMessage    *imsg;
	BOOL                    done = FALSE;

	/* Seed the random number generator */
	srand(0xDEADBEEF);

	cairo_win = create_window("text", 640, 480);
	if (cairo_win == NULL)
		return RETURN_ERROR;

	draw_text(cairo_win);
	refresh_window(cairo_win);

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

			}

			IExec->ReplyMsg((struct Message *)imsg);
		}
	}

	destroy_window(cairo_win);

	return RETURN_OK;
}

