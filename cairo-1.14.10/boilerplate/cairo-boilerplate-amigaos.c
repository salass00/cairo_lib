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

#include "cairo-boilerplate-private.h"

#include <cairo-amigaos-private.h>

#include <proto/graphics.h>

static cairo_surface_t *
_cairo_boilerplate_amigaos_create_surface (const char                *name,
                                           cairo_content_t            content,
                                           double                     width,
                                           double                     height,
                                           double                     max_width,
                                           double                     max_height,
                                           cairo_boilerplate_mode_t   mode,
                                           void                     **closure)
{
	cairo_amigaos_surface_t *surface;
	int                      depth;
	uint32                   pixfmt;
	struct BitMap           *bitmap;

	switch (content) {
		case CAIRO_CONTENT_COLOR:
			depth  = 24;
			pixfmt = PIXF_R8G8B8;
			break;

		case CAIRO_CONTENT_ALPHA:
			depth  = 8;
			pixfmt = PIXF_ALPHA8;
			break;

		case CAIRO_CONTENT_COLOR_ALPHA:
			depth  = 24;
			pixfmt = PIXF_A8R8G8B8;
			break;

		default:
			return NULL;
	}

	bitmap = IGraphics->AllocBitMapTags(width, height, depth,
		BMATags_PixelFormat, pixfmt,
		TAG_END);

	surface = (cairo_amigaos_surface_t *)cairo_amigaos_surface_create(bitmap);
	surface->free_bitmap = TRUE;

	*closure = NULL;

	return &surface->base;
}

static const cairo_boilerplate_target_t targets[] = {
    {
	"amigaos", "amigaos", NULL, NULL,
	CAIRO_SURFACE_TYPE_AMIGAOS, CAIRO_CONTENT_COLOR, 0,
	"cairo_amigaos_surface_create",
	_cairo_boilerplate_amigaos_create_surface,
	cairo_surface_create_similar,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	NULL, NULL, NULL, TRUE, FALSE, FALSE
    },
    {
	"amigaos", "amigaos", NULL, NULL,
	CAIRO_SURFACE_TYPE_AMIGAOS, CAIRO_CONTENT_COLOR_ALPHA, 0,
	"cairo_amigaos_surface_create",
	_cairo_boilerplate_amigaos_create_surface,
	cairo_surface_create_similar,
	NULL, NULL,
	_cairo_boilerplate_get_image_surface,
	cairo_surface_write_to_png,
	NULL, NULL, NULL, FALSE, FALSE, FALSE
    },
};
CAIRO_BOILERPLATE (amigaos, targets)
