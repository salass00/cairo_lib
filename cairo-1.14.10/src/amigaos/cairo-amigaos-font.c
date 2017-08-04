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
#include "cairo-image-surface-private.h"

#include <diskfont/diskfonttag.h>
#include <diskfont/oterrors.h>
#include <proto/diskfont.h>
#include <proto/utility.h>

#define FLOAT_TO_FIXED(x) ((int32)((x) * 0x00010000L))
#define FIXED_TO_FLOAT(x) ((float)(x) / 0x00010000L)

typedef struct _cairo_amigaos_scaled_font {
	cairo_scaled_font_t  base;

	struct OutlineFont  *outline_font;

	BOOL                 antialias:1;
	BOOL                 ucs2_only:1;

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

static struct GlyphMap *
_get_glyph_map (cairo_amigaos_scaled_font_t *font,
                uint32_t                     unicode)
{
	struct EGlyphEngine *engine = &font->outline_font->olf_EEngine;
	struct GlyphMap     *gm;

	if (font->ucs2_only && unicode > 0xFFFF)
		return NULL;

	IDiskfont->ESetInfo(engine,
	                    OT_GlyphCode, unicode,
	                    TAG_END);

	IDiskfont->EObtainInfo(engine,
	                       font->antialias ? OT_GlyphMap8Bit : OT_GlyphMap, &gm,
	                       TAG_END);

	return gm;
}

static void
_release_glyph_map (cairo_amigaos_scaled_font_t *font,
                    struct GlyphMap             *gm)
{
	struct EGlyphEngine *engine = &font->outline_font->olf_EEngine;

	if (gm == NULL)
		return;

	IDiskfont->EReleaseInfo(engine,
	                        OT_GlyphMap, gm,
	                        TAG_END);
}

static cairo_status_t
_cairo_amigaos_scaled_font_glyph_init_metrics (cairo_amigaos_scaled_font_t *font,
                                               cairo_scaled_glyph_t        *glyph,
                                               struct GlyphMap             *gm)
{
	cairo_text_extents_t extents;

	if (gm != NULL) {
		extents.x_bearing = gm->glm_X0;
		extents.y_bearing = -gm->glm_Y0;
		extents.width     = gm->glm_BlackWidth;
		extents.height    = gm->glm_BlackHeight;
		extents.x_advance = FIXED_TO_FLOAT(gm->glm_Width);
		extents.y_advance = 0;
	} else {
		extents.x_bearing = 0;
		extents.y_bearing = 0;
		extents.width     = 0;
		extents.height    = 0;
		extents.x_advance = font->xspace;
		extents.y_bearing = 0;
	}

	_cairo_scaled_glyph_set_metrics(glyph, &font->base, &extents);

	return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_amigaos_scaled_font_glyph_init_surface_a8 (cairo_amigaos_scaled_font_t *font,
                                                  cairo_scaled_glyph_t        *glyph,
                                                  struct GlyphMap             *gm)
{
	cairo_image_surface_t *surface;
	uint8_t               *src, *dst;
	int                    src_mod, dst_mod;
	int                    x, y;

	if (gm == NULL) {
		surface = (cairo_image_surface_t *)cairo_image_surface_create (CAIRO_FORMAT_A8, 1, 1);
		if (surface->base.status != CAIRO_STATUS_SUCCESS)
			return surface->base.status;

		_cairo_scaled_glyph_set_surface(glyph, &font->base, surface);

		return CAIRO_STATUS_SUCCESS;
	}

	surface = (cairo_image_surface_t *)cairo_image_surface_create (CAIRO_FORMAT_A8,
	                                                               gm->glm_BlackWidth,
	                                                               gm->glm_BlackHeight);
	if (surface->base.status != CAIRO_STATUS_SUCCESS)
		return surface->base.status;

	src = gm->glm_BitMap + (gm->glm_BMModulo * gm->glm_BlackTop) + gm->glm_BlackLeft;
	dst = surface->data;

	src_mod = gm->glm_BMModulo - gm->glm_BlackWidth;
	dst_mod = surface->stride - gm->glm_BlackWidth;

	for (y = 0; y < gm->glm_BlackHeight; y++) {
		for (x = 0; x < gm->glm_BlackWidth; x++)
			*dst++ = *src++;

		src += src_mod;
		dst += dst_mod;
	}

	cairo_surface_set_device_offset(&surface->base, 0, gm->glm_Y0);

	_cairo_scaled_glyph_set_surface(glyph, &font->base, surface);

	return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_amigaos_scaled_font_glyph_init_surface_a1 (cairo_amigaos_scaled_font_t *font,
                                                  cairo_scaled_glyph_t        *glyph,
                                                  struct GlyphMap             *gm)
{
	cairo_image_surface_t *surface;
	uint8_t               *src, *dst;
	int                    y, byte, byteoff;
	unsigned int           shift;

	if (gm == NULL) {
		surface = (cairo_image_surface_t *)cairo_image_surface_create (CAIRO_FORMAT_A1, 1, 1);
		if (surface->base.status != CAIRO_STATUS_SUCCESS)
			return surface->base.status;

		_cairo_scaled_glyph_set_surface(glyph, &font->base, surface);

		return CAIRO_STATUS_SUCCESS;
	}

	surface = (cairo_image_surface_t *)cairo_image_surface_create (CAIRO_FORMAT_A1,
	                                                               gm->glm_BlackWidth,
	                                                               gm->glm_BlackHeight);
	if (surface->base.status != CAIRO_STATUS_SUCCESS)
		return surface->base.status;

	src = gm->glm_BitMap + (gm->glm_BMModulo * gm->glm_BlackTop);
	dst = surface->data;

	byteoff = gm->glm_BlackLeft >> 3;
	shift   = gm->glm_BlackLeft & 7;

	for (y = 0; y < gm->glm_BlackHeight; y++) {
		for (byte = 0; byte < ((gm->glm_BlackWidth + 7) >> 3); byte++)
			dst[byte] = (src[byte + byteoff] << shift) | (src[byte + byteoff + 1] >> (8 - shift));

		src += gm->glm_BMModulo;
		dst += surface->stride;
	}

	cairo_surface_set_device_offset(&surface->base, 0, gm->glm_Y0);

	_cairo_scaled_glyph_set_surface(glyph, &font->base, surface);

	return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_amigaos_scaled_font_glyph_init (void                      *abstract_font,
                                       cairo_scaled_glyph_t      *glyph,
                                       cairo_scaled_glyph_info_t  info)
{
	cairo_amigaos_scaled_font_t *font = abstract_font;
	struct GlyphMap             *gm;
	cairo_status_t               status;

	if (info & CAIRO_SCALED_GLYPH_INFO_PATH)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	gm = _get_glyph_map(font, _cairo_scaled_glyph_index(glyph));

	if (info & CAIRO_SCALED_GLYPH_INFO_METRICS) {
		status = _cairo_amigaos_scaled_font_glyph_init_metrics(font, glyph, gm);
		if (status != CAIRO_STATUS_SUCCESS) {
			_release_glyph_map(font, gm);
			return status;
		}
	}

	if (info & CAIRO_SCALED_GLYPH_INFO_SURFACE) {
		if (font->antialias)
			status = _cairo_amigaos_scaled_font_glyph_init_surface_a8(font, glyph, gm);
		else
			status = _cairo_amigaos_scaled_font_glyph_init_surface_a1(font, glyph, gm);

		if (status != CAIRO_STATUS_SUCCESS) {
			_release_glyph_map(font, gm);
			return status;
		}
	}

	_release_glyph_map(font, gm);

	return CAIRO_STATUS_SUCCESS;
}

static int32_t
_get_kern (cairo_amigaos_scaled_font_t *font,
           uint32_t                     unicode,
           uint32_t                     unicode2)
{
	struct EGlyphEngine *engine = &font->outline_font->olf_EEngine;
	int32_t              kern;

	if (font->ucs2_only && (unicode > 0xFFFF || unicode2 > 0xFFFF))
		return 0;

	IDiskfont->ESetInfo(engine,
	                    OT_GlyphCode,  unicode,
	                    OT_GlyphCode2, unicode2,
	                    TAG_END);

	IDiskfont->EObtainInfo(engine,
	                       OT_TextKernPair, &kern,
	                       TAG_END);

	return kern;
}

static cairo_int_status_t
_cairo_amigaos_scaled_font_text_to_glyphs (void                        *abstract_font,
                                           double                       x,
                                           double                       y,
                                           const char                  *utf8,
                                           int                          utf8_len,
                                           cairo_glyph_t              **glyphs_out,
                                           int                         *num_glyphs,
                                           cairo_text_cluster_t       **clusters,
                                           int                         *num_clusters,
                                           cairo_text_cluster_flags_t  *cluster_flags)
{
	cairo_amigaos_scaled_font_t *font = abstract_font;
	uint32_t                    *ucs4;
	int                          len, i, j;
	cairo_glyph_t               *glyphs;
	struct GlyphMap             *gm;
	cairo_status_t               status;

	status = _cairo_utf8_to_ucs4(utf8, -1, &ucs4, &len);
	if (status != CAIRO_STATUS_SUCCESS)
		return status;

	glyphs = _cairo_malloc_ab(len, sizeof(cairo_glyph_t));

	for (i = j = 0; i < len; i++) {
		gm = _get_glyph_map(font, ucs4[i]);
		if (gm == NULL) {
			x += font->xspace;
			continue;
		}

		if (i > 0) {
			x -= gm->glm_X0 + FIXED_TO_FLOAT(_get_kern(font, ucs4[i - 1], ucs4[i]));
		}

		glyphs[j].index = ucs4[i];
		glyphs[j].x     = x;
		glyphs[j].y     = y;

		x += gm->glm_X1 - gm->glm_X0;

		_release_glyph_map(font, gm);

		j++;
	}

	free(ucs4);

	*glyphs_out = glyphs;
	*num_glyphs = j;

	return CAIRO_STATUS_SUCCESS;
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
	struct EGlyphEngine         *engine;
	cairo_amigaos_scaled_font_t *font;

	cairo_matrix_multiply(&scale, font_matrix, ctm);
	status = _cairo_matrix_compute_basis_scale_factors(&scale, &xscale, &yscale, 1);
	if (status != CAIRO_STATUS_SUCCESS)
		return status;

	outline_font = IDiskfont->OpenOutlineFont(face->filename, NULL, OFF_OPEN);
	if (outline_font == NULL)
		return _cairo_error(CAIRO_STATUS_NO_MEMORY);

	engine = &outline_font->olf_EEngine;

	font = malloc(sizeof(cairo_amigaos_scaled_font_t));

	font->outline_font = outline_font;

	font->xscale = xscale;
	font->yscale = yscale;

	IDiskfont->ESetInfo(engine,
	                    OT_PointHeight, FLOAT_TO_FIXED(font->yscale),
	                    TAG_END);

	if (IDiskfont->ESetInfo(engine,
	                        OT_GlyphCode_32, 'A',
	                        TAG_END) == OTERR_UnknownTag)
	{
		font->ucs2_only = TRUE;
	} else {
		font->ucs2_only = FALSE;
	}

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

