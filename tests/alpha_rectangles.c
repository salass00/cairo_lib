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
draw_rectangles(cairo_amigaos_window_t *cairo_win, int num)
{
	double red, green, blue, alpha;
	double x, y, w, h;
	int    i;

	for (i = 0; i < num; i++) {
		red   = (rand() % 256) / 255.0;
		green = (rand() % 256) / 255.0;
		blue  = (rand() % 256) / 255.0;
		alpha = (16 + (rand() % 240)) / 255.0;

		cairo_set_source_rgba(cairo_win->cairo, red, green, blue, alpha);

		x = rand() % cairo_win->width;
		y = rand() % cairo_win->height;
		w = rand() % (cairo_win->width / 2);
		h = rand() % (cairo_win->height / 2);

		cairo_rectangle(cairo_win->cairo, x, y, w, h);

		cairo_fill(cairo_win->cairo);
	}
}

int main(void) {
	cairo_amigaos_window_t *cairo_win;
	struct Window          *window;
	struct IntuiMessage    *imsg;
	BOOL                    done = FALSE;

	/* Seed the random number generator */
	srand(0xDEADBEEF);

	cairo_win = create_window("alpha_rectangles", 640, 480);
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

