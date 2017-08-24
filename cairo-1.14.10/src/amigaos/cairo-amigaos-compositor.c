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
#include "cairo-tristrip-private.h"
#include "cairo-traps-private.h"

#include <graphics/composite.h>
#include <proto/graphics.h>
#include <proto/layers.h>

#define COMPOSITE_Invalid (~0UL)

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
clipped_composite_func (struct Hook            *hook,
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
CompositeRastPortTagList (uint32                  op,
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
CompositeRastPortTags (uint32                  op,
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

static void
clip_and_init_bounds (const cairo_amigaos_surface_t *surface,
                      struct Rectangle              *bounds,
                      float                          x1,
                      float                          y1,
                      float                          x2,
                      float                          y2)
{
	int _x1, _y1, _x2, _y2;

	_x1 = (int)floorf(x1);
	_y1 = (int)floorf(y1);
	_x2 = (int)ceilf(x2);
	_y2 = (int)ceilf(y2);

	if (_x1 < 0)
		_x1 = 0;
	if (_y1 < 0)
		_y1 = 0;
	if (_x2 >= surface->width)
		_x2 = surface->width - 1;
	if (_y2 >= surface->height)
		_y2 = surface->height - 1;

	bounds->MinX = surface->xoff + _x1;
	bounds->MinY = surface->yoff + _y1;
	bounds->MaxX = surface->xoff + _x2;
	bounds->MaxY = surface->yoff + _y2;
}

static uint32
convert_operator_to_amigaos (cairo_operator_t op)
{
	uint32 amigaos_op;

	switch (op) {
		case CAIRO_OPERATOR_SOURCE:
			amigaos_op = COMPOSITE_Src;
			break;

		case CAIRO_OPERATOR_OVER:
			amigaos_op = COMPOSITE_Src_Over_Dest;
			break;

		case CAIRO_OPERATOR_CLEAR:
		case CAIRO_OPERATOR_IN:
		case CAIRO_OPERATOR_OUT:
		case CAIRO_OPERATOR_ATOP:
		case CAIRO_OPERATOR_DEST:
		case CAIRO_OPERATOR_DEST_OVER:
		case CAIRO_OPERATOR_DEST_IN:
		case CAIRO_OPERATOR_DEST_OUT:
		case CAIRO_OPERATOR_DEST_ATOP:
		case CAIRO_OPERATOR_XOR:
		case CAIRO_OPERATOR_ADD:
		case CAIRO_OPERATOR_SATURATE:
		case CAIRO_OPERATOR_MULTIPLY:
		case CAIRO_OPERATOR_SCREEN:
		case CAIRO_OPERATOR_OVERLAY:
		case CAIRO_OPERATOR_DARKEN:
		case CAIRO_OPERATOR_LIGHTEN:
		case CAIRO_OPERATOR_COLOR_DODGE:
		case CAIRO_OPERATOR_COLOR_BURN:
		case CAIRO_OPERATOR_HARD_LIGHT:
		case CAIRO_OPERATOR_SOFT_LIGHT:
		case CAIRO_OPERATOR_DIFFERENCE:
		case CAIRO_OPERATOR_EXCLUSION:
		case CAIRO_OPERATOR_HSL_HUE:
		case CAIRO_OPERATOR_HSL_SATURATION:
		case CAIRO_OPERATOR_HSL_COLOR:
		case CAIRO_OPERATOR_HSL_LUMINOSITY:
		default:
			amigaos_op = COMPOSITE_Invalid;
			break;
	}

	return amigaos_op;
}

static cairo_surface_t *
pattern_to_amigaos_surface (const cairo_pattern_t *pattern)
{
	struct BitMap           *bitmap;
	cairo_amigaos_surface_t *surface;

	switch (pattern->type) {
		case CAIRO_PATTERN_TYPE_SOLID:
		{
			cairo_color_t *color = &((cairo_solid_pattern_t *)pattern)->color;
			uint32         argb;

			bitmap = IGraphics->AllocBitMapTags(1, 1, 24,
				BMATags_PixelFormat, PIXF_A8R8G8B8,
				TAG_END);
			if (bitmap == NULL)
				return _cairo_surface_create_in_error(_cairo_error(CAIRO_STATUS_NO_MEMORY));

			surface = (cairo_amigaos_surface_t *)cairo_amigaos_surface_create(bitmap);
			if (unlikely (surface->base.backend == NULL)) {
				IGraphics->FreeBitMap(bitmap);
				return &surface->base;
			}

			surface->free_bitmap = TRUE;

			argb = ARGB(color->alpha_short >> 8, color->red * 255, color->green * 255, color->blue * 255);

			IGraphics->WritePixelColor(surface->rastport, 0, 0, argb);

			return &surface->base;
		}

		case CAIRO_PATTERN_TYPE_SURFACE:
		{
			cairo_surface_t *surface = ((cairo_surface_pattern_t *)pattern)->surface;

			if (surface->type != CAIRO_SURFACE_TYPE_AMIGAOS)
				return _cairo_int_surface_create_in_error(CAIRO_INT_STATUS_UNSUPPORTED);

			cairo_surface_reference(surface);

			return surface;
		}

		case CAIRO_PATTERN_TYPE_LINEAR:
		case CAIRO_PATTERN_TYPE_RADIAL:
		case CAIRO_PATTERN_TYPE_MESH:
		case CAIRO_PATTERN_TYPE_RASTER_SOURCE:
		default:
			return _cairo_int_surface_create_in_error(CAIRO_INT_STATUS_UNSUPPORTED);
	}
}

struct composite_data {
	uint32                   op;
	cairo_amigaos_surface_t *src;
	cairo_amigaos_surface_t *dst;
	uint32                   alpha;
};

static cairo_bool_t
composite_box (cairo_box_t *box, void *user_data)
{
	struct composite_data *cd = user_data;
	float                  x1, y1, x2, y2;
	struct Rectangle       bounds;
	my_vertex_t            vertices[4];
	uint16                 indices[6];
	uint32                 error;

	x1 = _cairo_fixed_to_double(box->p1.x);
	y1 = _cairo_fixed_to_double(box->p1.y);
	x2 = _cairo_fixed_to_double(box->p2.x);
	y2 = _cairo_fixed_to_double(box->p2.y);

	clip_and_init_bounds(cd->dst, &bounds, x1, y1, x2, y2);

	vertices[0] = VERTEX(x1, y1, 0, 0, 1);
	vertices[1] = VERTEX(x2, y1, 0, 0, 1);
	vertices[2] = VERTEX(x1, y2, 0, 0, 1);
	vertices[3] = VERTEX(x2, y2, 0, 0, 1);

	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 2;
	indices[4] = 1;
	indices[5] = 3;

	error = CompositeRastPortTags(cd->op, cd->src->bitmap, cd->dst->rastport, &bounds,
		COMPTAG_Flags,        COMPFLAG_HardwareOnly,
		COMPTAG_IndexArray,   indices,
		COMPTAG_VertexArray,  vertices,
		COMPTAG_VertexFormat, COMPVF_STW0_Present,
		COMPTAG_NumTriangles, 2,
		COMPTAG_SrcAlpha,     cd->alpha,
		TAG_END);

	if (unlikely (error != COMPERR_Success))
		return FALSE;

	return TRUE;
}

static cairo_int_status_t
composite_boxes (cairo_composite_rectangles_t *extents,
                 cairo_boxes_t                *boxes)
{
	cairo_int_status_t       status = CAIRO_INT_STATUS_UNSUPPORTED;
	uint32                   op;
	cairo_amigaos_surface_t *dst;
	const cairo_pattern_t   *src_pattern;
	cairo_amigaos_surface_t *src;
	struct composite_data    cd;

	op = convert_operator_to_amigaos(extents->op);
	if (op == COMPOSITE_Invalid)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	dst = (cairo_amigaos_surface_t *)extents->surface;

	src_pattern = &extents->source_pattern.base;
	if (src_pattern->type != CAIRO_PATTERN_TYPE_SOLID)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	src = (cairo_amigaos_surface_t *)pattern_to_amigaos_surface(src_pattern);
	if (src->base.backend == NULL)
		return src->base.status;

	cd.op    = op;
	cd.src   = src;
	cd.dst   = dst;
	cd.alpha = COMP_FIX_ONE;

	if (likely (_cairo_boxes_for_each_box(boxes, composite_box, &cd)))
		status = CAIRO_STATUS_SUCCESS;

	cairo_surface_destroy(&src->base);

	return status;
}

static cairo_int_status_t
composite_boxes_with_mask (cairo_composite_rectangles_t *extents,
                           cairo_boxes_t                *boxes)
{
	cairo_int_status_t       status = CAIRO_INT_STATUS_UNSUPPORTED;
	uint32                   op;
	cairo_amigaos_surface_t *dst;
	const cairo_pattern_t   *src_pattern;
	const cairo_pattern_t   *mask_pattern;
	cairo_amigaos_surface_t *src;
	struct composite_data    cd;

	op = convert_operator_to_amigaos(extents->op);
	if (op == COMPOSITE_Invalid)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	dst = (cairo_amigaos_surface_t *)extents->surface;

	src_pattern = &extents->source_pattern.base;
	if (src_pattern->type != CAIRO_PATTERN_TYPE_SOLID)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	mask_pattern = &extents->mask_pattern.base;
	if (mask_pattern->type != CAIRO_PATTERN_TYPE_SOLID)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	src = (cairo_amigaos_surface_t *)pattern_to_amigaos_surface(src_pattern);
	if (src->base.backend == NULL)
		return src->base.status;

	cd.op    = op;
	cd.src   = src;
	cd.dst   = dst;
	cd.alpha = COMP_FLOAT_TO_FIX(((cairo_solid_pattern_t *)mask_pattern)->color.alpha);

	if (likely (_cairo_boxes_for_each_box(boxes, composite_box, &cd)))
		status = CAIRO_STATUS_SUCCESS;

	cairo_surface_destroy(&src->base);

	return status;
}

static cairo_int_status_t
composite_tristrip (cairo_composite_rectangles_t *extents,
                    cairo_tristrip_t             *strip)
{
	cairo_int_status_t       status = CAIRO_INT_STATUS_UNSUPPORTED;
	uint32                   op;
	cairo_amigaos_surface_t *dst;
	const cairo_pattern_t   *src_pattern;
	cairo_amigaos_surface_t *src;
	float                    x1, y1, x2, y2;
	float                    x, y;
	my_vertex_t             *vertices;
	uint16                  *indices;
	int                      num_vertices, num_triangles;
	int                      i, j;
	struct Rectangle         bounds;
	uint32                   error;

	op = convert_operator_to_amigaos(extents->op);
	if (op == COMPOSITE_Invalid)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	dst = (cairo_amigaos_surface_t *)extents->surface;

	src_pattern = &extents->source_pattern.base;
	if (src_pattern->type != CAIRO_PATTERN_TYPE_SOLID)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	num_vertices = strip->num_points;
	if (num_vertices < 3)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	src = (cairo_amigaos_surface_t *)pattern_to_amigaos_surface(src_pattern);
	if (src->base.backend == NULL)
		return src->base.status;

	num_triangles = num_vertices - 2;

	vertices = malloc(sizeof(my_vertex_t) * num_vertices);
	indices  = malloc(sizeof(uint16) * 3 * num_triangles);


	x = _cairo_fixed_to_double(strip->points[0].x);
	y = _cairo_fixed_to_double(strip->points[0].y);

	x1 = x2 = x;
	y1 = y2 = y;

	vertices[0] = VERTEX(x, y, 0, 0, 1);

	for (i = 1; i < num_vertices; i++) {
		x = _cairo_fixed_to_double(strip->points[i].x);
		y = _cairo_fixed_to_double(strip->points[i].y);

		if (x < x1)
			x1 = x;
		else if (x > x2)
			x2 = x;
		if (y < y1)
			y1 = y;
		else if (y > y2)
			y2 = y;

		vertices[i] = VERTEX(x, y, 0, 0, 1);
	}

	clip_and_init_bounds(dst, &bounds, x1, y1, x2, y2);

	for (i = 0; i < num_triangles; i++) {
		j = i * 3;
		if ((i & 1) == 0) {
			indices[j + 0] = i + 0;
			indices[j + 1] = i + 1;
			indices[j + 2] = i + 2;
		} else {
			indices[j + 0] = i + 1;
			indices[j + 1] = i + 0;
			indices[j + 2] = i + 2;
		}
	}

	error = CompositeRastPortTags(op, src->bitmap, dst->rastport, &bounds,
		COMPTAG_Flags,        COMPFLAG_HardwareOnly,
		COMPTAG_IndexArray,   indices,
		COMPTAG_VertexArray,  vertices,
		COMPTAG_VertexFormat, COMPVF_STW0_Present,
		COMPTAG_NumTriangles, num_triangles,
		TAG_END);

	if (likely (error == COMPERR_Success))
		status = CAIRO_STATUS_SUCCESS;

	free(vertices);
	free(indices);

	cairo_surface_destroy(&src->base);

	return status;
}

static cairo_int_status_t
composite_traps (cairo_composite_rectangles_t *extents,
                 cairo_traps_t                *traps)
{
	return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_amigaos_compositor_paint (const cairo_compositor_t     *_compositor,
                                 cairo_composite_rectangles_t *extents)
{
	cairo_int_status_t status;
	cairo_boxes_t      boxes;

	_cairo_clip_steal_boxes(extents->clip, &boxes);
	status = composite_boxes (extents, &boxes);
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
	status = composite_boxes_with_mask (extents, &boxes);
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
			status = composite_boxes(extents, &boxes);
		_cairo_boxes_fini(&boxes);
	}

	if (status == CAIRO_INT_STATUS_UNSUPPORTED && _cairo_clip_is_region(extents->clip)) {
		cairo_tristrip_t strip;

		_cairo_tristrip_init_with_clip(&strip, extents->clip);
		status = _cairo_path_fixed_stroke_to_tristrip(path,
		                                              style,
		                                              ctm,
		                                              ctm_inverse,
		                                              tolerance,
		                                              &strip);
		if (likely (status == CAIRO_INT_STATUS_SUCCESS))
			status = composite_tristrip(extents, &strip);
		_cairo_tristrip_fini (&strip);
	}

	if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
		cairo_traps_t traps;

		_cairo_traps_init_with_clip(&traps, extents->clip);
		status = _cairo_path_fixed_stroke_polygon_to_traps(path,
		                                                   style,
		                                                   ctm,
		                                                   ctm_inverse,
		                                                   tolerance,
		                                                   &traps);
		if (likely (status == CAIRO_INT_STATUS_SUCCESS))
			status = composite_traps(extents, &traps);
		_cairo_traps_fini (&traps);
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
			status = composite_boxes(extents, &boxes);
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
	return CAIRO_INT_STATUS_UNSUPPORTED;
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

