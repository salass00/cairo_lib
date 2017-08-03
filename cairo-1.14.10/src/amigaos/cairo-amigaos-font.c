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

#include "cairoint.h"

#include "cairo-amigaos-private.h"

#include <diskfont/diskfonttag.h>
#include <proto/diskfont.h>
#include <proto/utility.h>

#define FLOAT_TO_FIXED(x) ((int32)((x) * 0x00010000L))
#define FIXED_TO_FLOAT(x) ((float)(x) / 0x00010000L)

typedef struct _cairo_amigaos_scaled_font {
	cairo_scaled_font_t  base;

	struct OutlineFont  *outline_font;

	BOOL                 antialias:1;

	double               xscale, yscale;
	double               xspace;
} cairo_amigaos_scaled_font_t;

typedef struct _cairo_amigaos_font_face {
	cairo_font_face_t  base;

	char              *filename;
} cairo_amigaos_font_face_t;

static void
_cairo_amigaos_scaled_font_fini (void *abstract_font)
{
	cairo_amigaos_scaled_font_t *font = abstract_font;

	IDiskfont->CloseOutlineFont(font->outline_font, NULL);
	font->outline_font = NULL;
}

static cairo_int_status_t
_cairo_amigaos_scaled_font_glyph_init (void                      *abstract_font,
                                       cairo_scaled_glyph_t      *glyph,
                                       cairo_scaled_glyph_info_t  info)
{
	cairo_amigaos_scaled_font_t *font = abstract_font;
	struct GlyphMap             *gm;
	cairo_status_t               status;
}

static cairo_int_status_t
_cairo_amigaos_scaled_font_text_to_glyphs (void                        *abstract_font,
                                           double                       x,
                                           double                       y,
                                           const char                  *utf8,
                                           int                          utf8_len,
                                           cairo_glyph_t              **glyph,
                                           int                         *num_glyphs,
                                           cairo_text_cluster_t       **clusters,
                                           int                         *num_clusters,
                                           cairo_text_cluster_flags_t  *cluster_flags)
{
	cairo_amigaos_scaled_font_t *font = abstract_font;
	struct GlyphMap             *gm;
	cairo_status_t               status;
}

static const cairo_scaled_font_backend_t cairo_amigaos_scaled_font_backend = {
	.type                = CAIRO_FONT_TYPE_AMIGAOS,
	.fini                = _cairo_amigaos_scaled_font_fini,
	.scaled_glyph_init   = _cairo_amigaos_scaled_font_glyph_init,
	.text_to_glyphs      = _cairo_amigaos_scaled_font_text_to_glyphs,
	.ucs4_to_index       = NULL,
	.load_truetype_table = NULL,
	.index_to_ucs4       = NULL,
	.is_synthetic        = NULL,
	.index_to_glyph_name = NULL,
	.load_type1_data     = NULL
};

static cairo_bool_t
_cairo_amigaos_font_face_destroy (void *abstract_face)
{
	cairo_amigaos_font_face_t *face = abstract_face;

	free(face->filename);
	face->filename = NULL;

	return TRUE;
}

static cairo_status_t
_cairo_amigaos_font_face_scaled_font_create (void                        *abstract_face,
                                             const cairo_matrix_t        *font_matrix,
                                             const cairo_matrix_t        *ctm,
                                             const cairo_font_options_t  *options,
                                             cairo_scaled_font_t        **font_out)
{
	cairo_amigaos_font_face_t   *face = abstract_face;
	cairo_status_t               status;
	cairo_matrix_t               scale;
	double                       xscale, yscale;
	struct OutlineFont          *outline_font;
	cairo_amigaos_scaled_font_t *font;

	cairo_matrix_multiply(&scale, font_matrix, ctm);
	status = _cairo_matrix_compute_basis_scale_factors(&scale, &xscale, &yscale, 1);
	if (status)
		return status;

	outline_font = IDiskfont->OpenOutlineFont(face->filename, NULL, OFF_OPEN);
	if (outline_font == NULL)
		return _cairo_error(CAIRO_STATUS_NO_MEMORY);

	font = malloc(sizeof(cairo_amigaos_scaled_font_t));

	font->outline_font = outline_font;

	font->xscale = xscale;
	font->yscale = yscale;

	IDiskfont->ESetInfo(&outline_font->olf_EEngine,
	                    OT_PointHeight, FLOAT_TO_FIXED(font->yscale),
	                    TAG_END);

	font->xspace = FIXED_TO_FLOAT(IUtility->GetTagData(OT_SpaceWidth, 0, outline_font->olf_OTagList)) * font->xscale;

	switch(options->antialias) {
		case CAIRO_ANTIALIAS_DEFAULT:
		case CAIRO_ANTIALIAS_NONE:
			font->antialias = FALSE;
			break;

		case CAIRO_ANTIALIAS_GRAY:
		case CAIRO_ANTIALIAS_SUBPIXEL:
		case CAIRO_ANTIALIAS_FAST:
		case CAIRO_ANTIALIAS_GOOD:
		case CAIRO_ANTIALIAS_BEST:
			font->antialias = TRUE;
			break;
	}

	status = _cairo_scaled_font_init(&font->base, &face->base,
	                                 font_matrix, ctm, options,
	                                 &cairo_amigaos_scaled_font_backend);
	if (status) {
		IDiskfont->CloseOutlineFont(outline_font, NULL);
		free(font);
		return status;
	}

	return CAIRO_STATUS_SUCCESS;
}

static const cairo_font_face_backend_t cairo_amigaos_font_face_backend = {
	.type               = CAIRO_FONT_TYPE_AMIGAOS,
	.create_for_toy     = NULL,
	.destroy            = _cairo_amigaos_font_face_destroy,
	.scaled_font_create = _cairo_amigaos_font_face_scaled_font_create,
	.get_implementation = NULL
};

cairo_font_face_t *
cairo_amigaos_font_face_create (const char *filename)
{
	cairo_amigaos_font_face_t *face;

	face = malloc(sizeof(cairo_amigaos_font_face_t));

	face->filename = strdup(filename);

	_cairo_font_face_init(&face->base, &cairo_amigaos_font_face_backend);

	return &face->base;
}

