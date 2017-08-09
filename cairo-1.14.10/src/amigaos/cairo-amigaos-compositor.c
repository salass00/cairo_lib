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
#include "cairo-clip-inline.h"
#include "cairo-compositor-private.h"

#include <graphics/composite.h>
#include <proto/graphics.h>

#define ARGB(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

static cairo_bool_t
fill_box (cairo_box_t *box, void *user_data)
{
	cairo_amigaos_surface_t *dst = user_data;
	int x1, y1, x2, y2;

	x1 = _cairo_fixed_integer_part(box->p1.x);
	y1 = _cairo_fixed_integer_part(box->p1.y);
	x2 = _cairo_fixed_integer_part(box->p2.x);
	y2 = _cairo_fixed_integer_part(box->p2.y);

	IGraphics->RectFill(dst->rastport, x1, y1, x2, y2);

	return TRUE;
}

static uint32_t color_to_rgb(const cairo_color_t *c)
{
	return ARGB(0xff, c->red_short >> 8, c->green_short >> 8, c->blue_short >> 8);
}

static cairo_int_status_t
fill_boxes (cairo_amigaos_surface_t *dst,
            const cairo_pattern_t   *src,
            cairo_boxes_t           *boxes)
{
	const cairo_color_t *color = &((cairo_solid_pattern_t *)src)->color;
	cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;

	IGraphics->SetRPAttrs(dst->rastport,
	                      RPTAG_APenColor, color_to_rgb(color),
	                      TAG_END);

	if (likely (_cairo_boxes_for_each_box(boxes, fill_box, dst)))
		status = CAIRO_STATUS_SUCCESS;

	return status;
}

typedef struct {
	float x, y, s, t, w;
} amigaos_vertex_t;

struct alpha_box {
	struct BitMap *src, *dst;
	uint8_t        alpha;
};

static cairo_bool_t alpha_box (cairo_box_t *box, void *user_data)
{
	struct alpha_box *ab = user_data;
	amigaos_vertex_t  vertices[6];
	int               x1, y1, x2, y2;
	uint32            error;

	x1 = _cairo_fixed_integer_part(box->p1.x);
	y1 = _cairo_fixed_integer_part(box->p1.y);
	x2 = _cairo_fixed_integer_part(box->p2.x);
	y2 = _cairo_fixed_integer_part(box->p2.y);

	vertices[0].x = x1;
	vertices[0].y = y1;
	vertices[0].s = 0;
	vertices[0].t = 0;
	vertices[0].w = 1;

	vertices[1].x = x2;
	vertices[1].y = y1;
	vertices[1].s = 1;
	vertices[1].t = 0;
	vertices[1].w = 1;

	vertices[2].x = x1;
	vertices[2].y = y2;
	vertices[2].s = 0;
	vertices[2].t = 1;
	vertices[2].w = 1;

	vertices[3].x = x1;
	vertices[3].y = y2;
	vertices[3].s = 0;
	vertices[3].t = 1;
	vertices[3].w = 1;

	vertices[4].x = x2;
	vertices[4].y = y1;
	vertices[4].s = 1;
	vertices[4].t = 0;
	vertices[4].w = 1;

	vertices[5].x = x2;
	vertices[5].y = y2;
	vertices[5].s = 1;
	vertices[5].t = 1;
	vertices[5].w = 1;

	error = IGraphics->CompositeTags(COMPOSITE_Src_Over_Dest, ab->src, ab->dst,
		COMPTAG_Flags,        COMPFLAG_HardwareOnly,
		COMPTAG_VertexArray,  vertices,
		COMPTAG_VertexFormat, COMPVF_STW0_Present,
		COMPTAG_NumTriangles, 2,
		COMPTAG_SrcAlpha,     (ab->alpha * 0x10000UL) / 255,
		TAG_END);
	if (unlikely (error != COMPERR_Success))
		return FALSE;

	return TRUE;
}

static cairo_int_status_t
alpha_blend_boxes (cairo_amigaos_surface_t *dst,
                   const cairo_pattern_t   *src,
                   cairo_boxes_t           *boxes,
                   uint8_t                  alpha)
{
	const cairo_color_t *color  = &((cairo_solid_pattern_t *)src)->color;
	cairo_int_status_t   status = CAIRO_INT_STATUS_UNSUPPORTED;
	struct BitMap       *temp_bm;
	uint32_t            *bm_addr;
	APTR                 bm_lock;
	struct alpha_box     ab;

	temp_bm = IGraphics->AllocBitMapTags(1, 1, 24,
		BMATags_Friend,      dst->bitmap,
		BMATags_PixelFormat, PIXF_A8R8G8B8,
		TAG_END);
	if (temp_bm == NULL)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	bm_lock = IGraphics->LockBitMapTags(temp_bm,
		LBM_BaseAddress, &bm_addr,
		TAG_END);

	/* We don't use the short values here because they have the alpha premultiplied. */
	*bm_addr = ARGB(color->alpha * 255, color->red * 255, color->green * 255, color->blue * 255);

	IGraphics->UnlockBitMap(bm_lock);

	ab.src   = temp_bm;
	ab.dst   = dst->bitmap;
	ab.alpha = alpha;

	if (likely (_cairo_boxes_for_each_box(boxes, alpha_box, &ab)))
		status = CAIRO_STATUS_SUCCESS;

	IGraphics->FreeBitMap(temp_bm);

	return status;
}

static cairo_int_status_t
draw_boxes (cairo_composite_rectangles_t *extents,
            cairo_boxes_t                *boxes)
{
	cairo_operator_t         op  = extents->op;
	cairo_amigaos_surface_t *dst = (cairo_amigaos_surface_t *)extents->surface;
	const cairo_pattern_t   *src = &extents->source_pattern.base;

	if (boxes->num_boxes == 0 && extents->is_bounded)
		return CAIRO_STATUS_SUCCESS;

	if (!boxes->is_pixel_aligned)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	if (op == CAIRO_OPERATOR_CLEAR)
		op = CAIRO_OPERATOR_SOURCE;

	if (op == CAIRO_OPERATOR_OVER &&
	    _cairo_pattern_is_opaque(src, &extents->bounded))
		op = CAIRO_OPERATOR_SOURCE;

	if (dst->base.is_clear &&
	    (op == CAIRO_OPERATOR_OVER || op == CAIRO_OPERATOR_ADD))
		op = CAIRO_OPERATOR_SOURCE;

	if (src->type != CAIRO_PATTERN_TYPE_SOLID)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	if (op == CAIRO_OPERATOR_SOURCE)
		return fill_boxes(dst, src, boxes);

	if (op == CAIRO_OPERATOR_OVER)
		return alpha_blend_boxes(dst, src, boxes, 255);

	return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
opacity_boxes (cairo_composite_rectangles_t *extents,
               cairo_boxes_t                *boxes)
{
	cairo_operator_t         op  = extents->op;
	cairo_amigaos_surface_t *dst = (cairo_amigaos_surface_t *)extents->surface;
	const cairo_pattern_t   *src = &extents->source_pattern.base;

	if (extents->mask_pattern.base.type != CAIRO_PATTERN_TYPE_SOLID)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	if (!boxes->is_pixel_aligned)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	if (op != CAIRO_OPERATOR_OVER)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	return alpha_blend_boxes(dst, src, boxes,
	                         extents->mask_pattern.solid.color.alpha_short >> 8);
}

static cairo_int_status_t
_cairo_amigaos_compositor_paint (const cairo_compositor_t     *_compositor,
                                 cairo_composite_rectangles_t *extents)
{
	cairo_int_status_t status;
	cairo_boxes_t      boxes;

	_cairo_clip_steal_boxes(extents->clip, &boxes);
	status = draw_boxes (extents, &boxes);
	_cairo_clip_unsteal_boxes(extents->clip, &boxes);

	return status;
}

static cairo_int_status_t
_cairo_amigaos_compositor_mask (const cairo_compositor_t     *_compositor,
                                cairo_composite_rectangles_t *extents)
{
	cairo_int_status_t status;
	cairo_boxes_t      boxes;

	_cairo_clip_steal_boxes(extents->clip, &boxes);
	status = opacity_boxes (extents, &boxes);
	_cairo_clip_unsteal_boxes(extents->clip, &boxes);

	return status;
}

static cairo_int_status_t
_cairo_amigaos_compositor_stroke (const cairo_compositor_t     *_compositor,
                                  cairo_composite_rectangles_t *extents,
                                  const cairo_path_fixed_t     *path,
                                  const cairo_stroke_style_t   *style,
                                  const cairo_matrix_t         *ctm,
                                  const cairo_matrix_t         *ctm_inverse,
                                  double                        tolerance,
                                  cairo_antialias_t             antialias)
{
	cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;

	if (_cairo_path_fixed_fill_is_rectilinear(path)) {
		cairo_boxes_t boxes;

		_cairo_boxes_init_with_clip(&boxes, extents->clip);
		status = _cairo_path_fixed_stroke_rectilinear_to_boxes(path,
		                                                       style,
		                                                       ctm,
		                                                       antialias,
		                                                       &boxes);
		if (likely (status == CAIRO_INT_STATUS_SUCCESS))
			status = draw_boxes(extents, &boxes);
		_cairo_boxes_fini(&boxes);
	}

	return status;
}

static cairo_int_status_t
_cairo_amigaos_compositor_fill (const cairo_compositor_t     *_compositor,
                                cairo_composite_rectangles_t *extents,
                                const cairo_path_fixed_t     *path,
                                cairo_fill_rule_t             fill_rule,
                                double                        tolerance,
                                cairo_antialias_t             antialias)
{
	cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;

	if (_cairo_path_fixed_fill_is_rectilinear(path)) {
		cairo_boxes_t boxes;

		_cairo_boxes_init_with_clip(&boxes, extents->clip);
		status = _cairo_path_fixed_fill_rectilinear_to_boxes(path,
		                                                     fill_rule,
		                                                     antialias,
		                                                     &boxes);
		if (likely (status == CAIRO_INT_STATUS_SUCCESS))
			status = draw_boxes(extents, &boxes);
		_cairo_boxes_fini(&boxes);
	}

	return status;
}

static cairo_int_status_t
_cairo_amigaos_compositor_glyphs (const cairo_compositor_t     *_compositor,
                                  cairo_composite_rectangles_t *extents,
                                  cairo_scaled_font_t          *scaled_font,
                                  cairo_glyph_t                *glyphs,
                                  int                           num_glyphs,
                                  cairo_bool_t                  overlap)
{
	cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;

	return status;
}

const cairo_compositor_t _cairo_amigaos_compositor = {
	.delegate = &_cairo_fallback_compositor,
	.paint    = _cairo_amigaos_compositor_paint,
	.mask     = _cairo_amigaos_compositor_mask,
	.stroke   = _cairo_amigaos_compositor_stroke,
	.fill     = _cairo_amigaos_compositor_fill,
	.glyphs   = _cairo_amigaos_compositor_glyphs
};

