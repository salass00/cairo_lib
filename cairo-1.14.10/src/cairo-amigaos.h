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

#ifndef _CAIRO_AMIGAOS_H_
#define _CAIRO_AMIGAOS_H_

#include "cairo.h"

#ifdef CAIRO_HAS_AMIGAOS_SURFACE

# ifndef GRAPHICS_GFX_H
#  include <graphics/gfx.h>
# endif
# ifndef GRAPHICS_RASTPORT_H
#  include <graphics/rastport.h>
# endif

CAIRO_BEGIN_DECLS

cairo_public cairo_surface_t *
cairo_amigaos_surface_create (struct BitMap *bitmap);
cairo_public cairo_surface_t *
cairo_amigaos_surface_create_from_rastport (struct RastPort *rastport,
                                            int              xoff,
                                            int              yoff,
                                            int              width,
                                            int              height);

CAIRO_END_DECLS

#endif /* CAIRO_HAS_AMIGAOS_SURFACE */

#ifdef CAIRO_HAS_AMIGAOS_FONT

CAIRO_BEGIN_DECLS

cairo_public cairo_font_face_t *
cairo_amigaos_font_face_create (const char *filename);

CAIRO_END_DECLS

#endif /* CAIRO_HAS_AMIGAOS_FONT */

#if !defined(CAIRO_HAS_AMIGAOS_SURFACE) && !defined(CAIRO_HAS_AMIGAOS_FONT)
# error Cairo was not compiled with support for the AmigaOS surface or font backends
#endif

#endif /* _CAIRO_AMIGAOS_H_ */

