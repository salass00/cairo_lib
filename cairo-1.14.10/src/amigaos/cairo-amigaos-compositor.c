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
#include <proto/layers.h>

#define ARGB(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

typedef struct {
	float x, y, s, t, w;
} my_vertex_t;

#define VERTEX(_x, _y, _s, _t, _w) \
	({ \
		my_vertex_t vertex; \
		vertex.x = (_x); \
		vertex.y = (_y); \
		vertex.s = (_s); \
		vertex.t = (_t); \
		vertex.w = (_w); \
		vertex; \
	})

struct clipped_composite_data {
	uint32                op;
	struct BitMap        *src;
	const struct TagItem *tags;
	uint32                error;
};

static void
clipped_composite_func(struct Hook            *hook,
                       struct RastPort        *rp,
                       struct BackFillMessage *bfm)
{
	struct clipped_composite_data *ccd = hook->h_Data;

	if (unlikely (ccd->error != COMPERR_Success))
		return;

	ccd->error = IGraphics->CompositeTags(ccd->op, ccd->src, rp->BitMap,
		COMPTAG_DestX,      bfm->Bounds.MinX,
		COMPTAG_DestY,      bfm->Bounds.MinY,
		COMPTAG_DestWidth,  bfm->Bounds.MaxX - bfm->Bounds.MinX + 1,
		COMPTAG_DestHeight, bfm->Bounds.MaxY - bfm->Bounds.MinY + 1,
		TAG_MORE,           ccd->tags);
}

static uint32
CompositeRastPortTagList(uint32                  op,
                         struct BitMap          *src_bm,
                         struct RastPort        *dst_rp,
                         const struct Rectangle *bounds,
                         const struct TagItem   *tags)
{
	uint32 error;

	if (dst_rp->Layer == NULL) {
		error = IGraphics->CompositeTagList(op, src_bm, dst_rp->BitMap, tags);
	} else {
		struct clipped_composite_data ccd;
		struct Hook                   hook;

		ccd.op    = op;
		ccd.src   = src_bm;
		ccd.tags  = tags;
		ccd.error = COMPERR_Success;

		hook.h_Entry = (HOOKFUNC)clipped_composite_func;
		hook.h_Data  = &ccd;

		ILayers->DoHookClipRects(&hook, dst_rp, bounds);

		error = ccd.error;
	}

	return error;
}

static VARARGS68K uint32
CompositeRastPortTags(uint32                  op,
                      struct BitMap          *src_bm,
                      struct RastPort        *dst_rp,
                      const struct Rectangle *bounds,
                      ...)
{
	uint32  error;
	va_list tags;

	va_startlinear(tags, bounds);
	error = CompositeRastPortTagList(op, src_bm, dst_rp, bounds,
	                                 va_getlinearva(tags, const struct TagItem *));
	va_end(tags);

	return error;
}

static cairo_bool_t
fill_box (cairo_box_t *box, void *user_data)
{
	cairo_amigaos_surface_t *dst = user_data;
	int                      x1, y1, x2, y2;

	x1 = _cairo_fixed_integer_part(box->p1.x);
	y1 = _cairo_fixed_integer_part(box->p1.y);
	x2 = _cairo_fixed_integer_part(box->p2.x);
	y2 = _cairo_fixed_integer_part(box->p2.y);

	IGraphics->RectFill(dst->rastport, x1, y1, x2, y2);

	return TRUE;
}

static cairo_int_status_t
fill_boxes (cairo_amigaos_surface_t *dst,
            const cairo_pattern_t   *src,
            cairo_boxes_t           *boxes)
{
	const cairo_color_t *color  = &((cairo_solid_pattern_t *)src)->color;
	cairo_int_status_t   status = CAIRO_INT_STATUS_UNSUPPORTED;

	IGraphics->SetRPAttrs(dst->rastport,
	                      RPTAG_APenColor, ARGB(0xff, color->red_short >> 8, color->green_short >> 8, color->blue_short >> 8),
	                      TAG_END);

	if (likely (_cairo_boxes_for_each_box(boxes, fill_box, dst)))
		status = CAIRO_STATUS_SUCCESS;

	return status;
}

struct alpha_box {
	struct BitMap           *src_bm;
	cairo_amigaos_surface_t *dst;
	uint32                   alpha;
};

static cairo_bool_t
alpha_box (cairo_box_t *box, void *user_data)
{
	struct alpha_box *ab = user_data;
	int               x1, y1, x2, y2;
	struct Rectangle  rect;
	my_vertex_t       vertices[4];
	uint16            indices[6];
	uint32            error;

	x1 = _cairo_fixed_integer_part(box->p1.x);
	y1 = _cairo_fixed_integer_part(box->p1.y);
	x2 = _cairo_fixed_integer_part(box->p2.x);
	y2 = _cairo_fixed_integer_part(box->p2.y);

	rect.MinX = ab->dst->xoff + x1;
	rect.MinY = ab->dst->yoff + y1;
	rect.MaxX = ab->dst->xoff + x2;
	rect.MaxY = ab->dst->yoff + y2;

	vertices[0] = VERTEX(x1, y1, 0, 0, 0);
	vertices[1] = VERTEX(x2, y1, 0, 0, 0);
	vertices[2] = VERTEX(x1, y2, 0, 0, 0);
	vertices[3] = VERTEX(x2, y2, 0, 0, 0);

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 2;
	indices[4] = 1;
	indices[5] = 3;

	error = CompositeRastPortTags(COMPOSITE_Src_Over_Dest, ab->src_bm, ab->dst->rastport, &rect,
		COMPTAG_Flags,        COMPFLAG_HardwareOnly,
		COMPTAG_IndexArray,   indices,
		COMPTAG_VertexArray,  vertices,
		COMPTAG_VertexFormat, COMPVF_STW0_Present,
		COMPTAG_NumTriangles, 2,
		COMPTAG_SrcAlpha,     ab->alpha,
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
	*bm_addr = ARGB(color->alpha_short >> 8, color->red * 255, color->green * 255, color->blue * 255);

	IGraphics->UnlockBitMap(bm_lock);

	ab.src_bm = temp_bm;
	ab.dst    = dst;
	ab.alpha  = (alpha * 0x10000UL) / 255;

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

const cairo_compositor_t *
_cairo_amigaos_compositor_get (void)
{
	static cairo_compositor_t compositor;

	if (compositor.delegate == NULL) {
		compositor.delegate = &_cairo_fallback_compositor;

		compositor.paint    = _cairo_amigaos_compositor_paint;
		compositor.mask     = _cairo_amigaos_compositor_mask;
		compositor.stroke   = _cairo_amigaos_compositor_stroke;
		compositor.fill     = _cairo_amigaos_compositor_fill;
		compositor.glyphs   = _cairo_amigaos_compositor_glyphs;
	}

	return &compositor;
}

