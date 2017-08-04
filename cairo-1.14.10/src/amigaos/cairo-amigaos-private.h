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

#ifndef CAIRO_AMIGAOS_PRIVATE_H
#define CAIRO_AMIGAOS_PRIVATE_H

#include "cairo-amigaos.h"

#include "cairoint.h"

#define DEBUG_AMIGAOS_SURFACES 0

typedef struct _cairo_amigaos_surface {
	cairo_surface_t base;

	struct RastPort       *rastport;
	struct BitMap         *bitmap;
	BOOL                   free_rastport:1;
	BOOL                   free_bitmap:1;

	cairo_content_t        content;

	int                    xoff, yoff;
	int                    width, height;

	cairo_rectangle_int_t  map_rect;
	APTR                   map_lock;
	uint32                 map_pixfmt;
	cairo_image_surface_t *map_image;
} cairo_amigaos_surface_t;

#if DEBUG_AMIGAOS_SURFACES
void _cairo_amigaos_debugf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#define debugf(fmt, args...) _cairo_amigaos_debugf(fmt, ## args)
#else
#define debugf(fmt, ...)
#endif

extern const cairo_compositor_t _cairo_amigaos_compositor;

#endif /* CAIRO_AMIGAOS_PRIVATE_H */
