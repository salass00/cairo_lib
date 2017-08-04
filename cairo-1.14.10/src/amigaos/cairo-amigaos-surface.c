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
#include "cairo-default-context-private.h"
#include "cairo-compositor-private.h"
#include "cairo-image-surface-private.h"

#include <proto/graphics.h>

#if DEBUG_AMIGAOS_SURFACES
#include <stdio.h>

void _cairo_amigaos_debugf(const char *fmt, ...)
{
	char buffer[256];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	IExec->DebugPrintF("%s", buffer);
}
#endif

/* Returns CAIRO_FORMAT_INVALID for pixel formats not directly supported by cairo. */
static cairo_format_t
_amigaos_pixel_format_to_cairo_format (uint32 pixfmt)
{
	cairo_format_t format;

	switch (pixfmt) {
		case PIXF_A8R8G8B8:
			format = CAIRO_FORMAT_ARGB32;
			break;

		case PIXF_ALPHA8:
		case PIXF_CLUT:
			format = CAIRO_FORMAT_A8;
			break;

		case PIXF_R5G6B5:
			format = CAIRO_FORMAT_RGB16_565;
			break;

		default:
			format = CAIRO_FORMAT_INVALID;
			break;
	}

	return format;
}

static cairo_status_t
_cairo_amigaos_surface_finish (void *abstract_surface)
{
	cairo_amigaos_surface_t *surface = abstract_surface;

	debugf("_cairo_amigaos_surface_finish(%p)\n", abstract_surface);

	if (surface->free_rastport) {
		free(surface->rastport);
		surface->rastport = NULL;
	}

	if (surface->free_bitmap) {
		IGraphics->FreeBitMap(surface->bitmap);
		surface->bitmap = NULL;
	}

	return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
_cairo_amigaos_surface_create_similar(void            *abstract_surface,
                                      cairo_content_t  content,
                                      int              width,
                                      int              height)
{
	cairo_amigaos_surface_t *src = abstract_surface, *surface;
	int                      depth;
	uint32                   pixfmt;
	struct BitMap           *bitmap;

	debugf("_cairo_amigaos_surface_create_similar(%p, 0x%x, %d, %d)\n",
	       abstract_surface, (unsigned)content, width, height);

	switch (content) {
		case CAIRO_CONTENT_COLOR:
			depth  = 24;
			pixfmt = PIXF_A8R8G8B8;
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
		BMATags_Friend,      src->bitmap,
		BMATags_PixelFormat, pixfmt,
		TAG_END);

	surface = (cairo_amigaos_surface_t *)cairo_amigaos_surface_create(bitmap);
	surface->free_bitmap = TRUE;

	return &surface->base;
}

static cairo_surface_t *
_cairo_amigaos_surface_create_similar_image (void           *abstract_surface,
                                             cairo_format_t  format,
                                             int             width,
                                             int             height)
{
	debugf("_cairo_amigaos_surface_create_similiar_image(%p, (int)%d, %d, %d)\n",
	        abstract_surface, format, width, height);

	return NULL;
}

static cairo_image_surface_t *
_cairo_amigaos_surface_map_to_image (void                        *abstract_surface,
                                     const cairo_rectangle_int_t *extents)
{
	cairo_amigaos_surface_t *surface = abstract_surface;
	int                      x, y, width, height;
	uint32                   pixfmt;
	cairo_format_t           format;
	int                      bpp, stride;
	uint8_t                 *data;
	cairo_image_surface_t   *image;

	debugf("_cairo_amigaos_surface_map_to_image(%p, %p)\n",
	       abstract_surface, extents);

	if (extents) {
		x      = extents->x;
		y      = extents->y;
		width  = extents->width;
		height = extents->height;
	} else {
		x      = 0;
		y      = 0;
		width  = surface->width;
		height = surface->height;
	}

	surface->map_rect.x      = x;
	surface->map_rect.y      = y;
	surface->map_rect.width  = width;
	surface->map_rect.height = height;

	pixfmt = IGraphics->GetBitMapAttr(surface->bitmap, BMA_PIXELFORMAT);

	switch (surface->content) {
		case CAIRO_CONTENT_COLOR:
			if (pixfmt == PIXF_R5G6B5) {
				bpp    = 2;
				format = CAIRO_FORMAT_RGB16_565;
			} else {
				bpp    = 4;
				pixfmt = PIXF_A8R8G8B8;
				format = CAIRO_FORMAT_RGB24;
			}
			break;

		case CAIRO_CONTENT_ALPHA:
			bpp    = 1;
			pixfmt = PIXF_ALPHA8;
			format = CAIRO_FORMAT_A8;
			break;

		case CAIRO_CONTENT_COLOR_ALPHA:
			bpp    = 4;
			pixfmt = PIXF_A8R8G8B8;
			format = CAIRO_FORMAT_ARGB32;
			break;

		default:
			return NULL;
	}

	stride = width * bpp;
	data   = (uint8_t *)malloc(height * stride);

	IGraphics->ReadPixelArray(surface->rastport, surface->xoff + x, surface->yoff + y,
	                          data, 0, 0, stride, pixfmt,
	                          width, height);

	image = (cairo_image_surface_t *)cairo_image_surface_create_for_data(data, format,
	                                                                     width, height,
	                                                                     stride);

	surface->map_pixfmt = pixfmt;
	surface->map_image  = image;

	return image;
}

static cairo_int_status_t
_cairo_amigaos_surface_unmap_image (void                  *abstract_surface,
                                    cairo_image_surface_t *image)
{
	cairo_amigaos_surface_t *surface = abstract_surface;
	int                      x, y, width, height;
	uint32                   pixfmt;
	int                      stride;
	uint8                   *data;

	debugf("_cairo_amigaos_surface_unmap_image(%p, %p)\n",
	       abstract_surface, image);

	x      = surface->map_rect.x;
	y      = surface->map_rect.y;
	width  = surface->map_rect.width;
	height = surface->map_rect.height;

	pixfmt = surface->map_pixfmt;
	stride = image->stride;
	data   = image->data;

	IGraphics->WritePixelArray(data, 0, 0, stride, pixfmt,
	                           surface->rastport, surface->xoff + x, surface->yoff + y,
	                           width, height);

	cairo_surface_destroy(&image->base);
	free(data);

	surface->map_image = NULL;

	return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_cairo_amigaos_surface_get_extents (void                  *abstract_surface,
                                    cairo_rectangle_int_t *rectangle)
{
	cairo_amigaos_surface_t *surface = abstract_surface;

	debugf("_cairo_amigaos_surface_get_extents(%p, %p)\n",
	       abstract_surface, rectangle);

	rectangle->x      = 0;
	rectangle->y      = 0;
	rectangle->width  = surface->width;
	rectangle->height = surface->height;

	return TRUE;
}

static cairo_int_status_t
_cairo_amigaos_surface_paint (void                  *surface,
                              cairo_operator_t       op,
                              const cairo_pattern_t *source,
                              const cairo_clip_t    *clip)
{
	debugf("_cairo_amigaos_surface_paint(%p, %d, %p, %p)\n",
	       surface, op, source, clip);

	return _cairo_compositor_paint (&_cairo_amigaos_compositor,
	                                surface, op, source, clip);
}

static cairo_int_status_t
_cairo_amigaos_surface_mask (void                  *surface,
                             cairo_operator_t       op,
                             const cairo_pattern_t *source,
                             const cairo_pattern_t *mask,
                             const cairo_clip_t    *clip)
{
	debugf("_cairo_amigaos_surface_mask(%p, %d, %p, %p, %p)\n",
	       surface, op, source, mask, clip);

	return _cairo_compositor_mask (&_cairo_amigaos_compositor,
	                               surface, op, source, mask, clip);
}

static cairo_int_status_t
_cairo_amigaos_surface_stroke (void                       *surface,
                               cairo_operator_t            op,
                               const cairo_pattern_t      *source,
                               const cairo_path_fixed_t   *path,
                               const cairo_stroke_style_t *style,
                               const cairo_matrix_t       *ctm,
                               const cairo_matrix_t       *ctm_inverse,
                               double                      tolerance,
                               cairo_antialias_t           antialias,
                               const cairo_clip_t         *clip)
{
	debugf("_cairo_amigaos_surface_stroke(%p, %d, %p, %p, %p, %p, %p, %f, %d, %p)\n",
	       surface, op, source, path, style, ctm, ctm_inverse, tolerance, antialias, clip);

	return _cairo_compositor_stroke (&_cairo_amigaos_compositor,
	                                 surface, op, source, path,
	                                 style, ctm,ctm_inverse,
	                                 tolerance, antialias, clip);
}

static cairo_int_status_t
_cairo_amigaos_surface_fill (void                     *surface,
                             cairo_operator_t          op,
                             const cairo_pattern_t    *source,
                             const cairo_path_fixed_t *path,
                             cairo_fill_rule_t         fill_rule,
                             double                    tolerance,
                             cairo_antialias_t         antialias,
                             const cairo_clip_t       *clip)
{
	debugf("_cairo_amigaos_surface_fill(%p, %d, %p, %p, %d, %f, %d, %p)\n",
	       surface, op, source, path, fill_rule, tolerance, antialias, clip);

	return _cairo_compositor_fill (&_cairo_amigaos_compositor,
	                               surface, op, source, path,
	                               fill_rule, tolerance, antialias,
	                               clip);
}

static cairo_int_status_t
_cairo_amigaos_surface_glyphs (void                  *surface,
                               cairo_operator_t       op,
                               const cairo_pattern_t *source,
                               cairo_glyph_t         *glyphs,
                               int                    num_glyphs,
                               cairo_scaled_font_t   *scaled_font,
                               const cairo_clip_t    *clip)
{
	debugf("_cairo_amigaos_surface_glyphs(%p, %d, %p, %p, %d, %p, %p)\n",
	       surface, op, source, glyphs, num_glyphs, scaled_font, clip);

	return _cairo_compositor_glyphs (&_cairo_amigaos_compositor,
	                                 surface, op, source,
	                                 glyphs, num_glyphs, scaled_font,
	                                 clip);
}

static const cairo_surface_backend_t cairo_amigaos_surface_backend = {
	.type                     = CAIRO_SURFACE_TYPE_AMIGAOS,
	.finish                   = _cairo_amigaos_surface_finish,
	.create_context           = _cairo_default_context_create,
	.create_similar           = _cairo_amigaos_surface_create_similar,
	.create_similar_image     = _cairo_amigaos_surface_create_similar_image,
	.map_to_image             = _cairo_amigaos_surface_map_to_image,
	.unmap_image              = _cairo_amigaos_surface_unmap_image,
	.source                   = _cairo_surface_default_source,
    .acquire_source_image     = _cairo_surface_default_acquire_source_image,
    .release_source_image     = _cairo_surface_default_release_source_image,
	.snapshot                 = NULL,
	.copy_page                = NULL,
	.show_page                = NULL,
	.get_extents              = _cairo_amigaos_surface_get_extents,
	.get_font_options         = NULL,
	.flush                    = NULL,
	.mark_dirty_rectangle     = NULL,
	.paint                    = _cairo_amigaos_surface_paint,
	.mask                     = _cairo_amigaos_surface_mask,
	.stroke                   = _cairo_amigaos_surface_stroke,
	.fill                     = _cairo_amigaos_surface_fill,
	.fill_stroke              = NULL,
	.show_glyphs              = _cairo_amigaos_surface_glyphs,
	.has_show_text_glyphs     = FALSE,
	.show_text_glyphs         = NULL,
	.get_supported_mime_types = NULL
};

cairo_surface_t *
cairo_amigaos_surface_create (struct BitMap *bitmap)
{
	cairo_amigaos_surface_t *surface;
	int                      width, height;
	struct RastPort         *rastport;

	debugf("cairo_amigaos_surface_create(%p)\n", bitmap);

	surface = (cairo_amigaos_surface_t *)malloc(sizeof(cairo_amigaos_surface_t));

	width  = IGraphics->GetBitMapAttr(bitmap, BMA_ACTUALWIDTH);
	height = IGraphics->GetBitMapAttr(bitmap, BMA_HEIGHT);

	rastport = (struct RastPort *)malloc(sizeof(struct RastPort));

	IGraphics->InitRastPort(rastport);
	rastport->BitMap = bitmap;

	surface = (cairo_amigaos_surface_t *)cairo_amigaos_surface_create_from_rastport(rastport, 0, 0, width, height);
	surface->free_rastport = TRUE;

	return &surface->base;
}

cairo_surface_t *
cairo_amigaos_surface_create_from_rastport (struct RastPort *rastport,
                                            int              xoff,
                                            int              yoff,
                                            int              width,
                                            int              height)
{
	cairo_amigaos_surface_t *surface;
	struct BitMap           *bitmap;
	uint32                   pixfmt;

	debugf("cairo_amigaos_surface_create_from_rastport(%p, %d, %d, %d, %d)\n", rastport, xoff, yoff, width, height);

	bitmap = rastport->BitMap;

	surface = (cairo_amigaos_surface_t *)malloc(sizeof(cairo_amigaos_surface_t));

	surface->rastport      = rastport;
	surface->bitmap        = bitmap;
	surface->free_rastport = FALSE;
	surface->free_bitmap   = FALSE;

	surface->xoff   = 0;
	surface->yoff   = 0;
	surface->width  = width;
	surface->height = height;

	pixfmt = IGraphics->GetBitMapAttr(bitmap, BMA_PIXELFORMAT);

	switch (pixfmt) {
		case PIXF_ALPHA8:
		case PIXF_CLUT:
			surface->content = CAIRO_CONTENT_ALPHA;
			break;

		case PIXF_A8R8G8B8:
		case PIXF_R8G8B8A8:
		case PIXF_B8G8R8A8:
		case PIXF_A8B8G8R8:
			surface->content = CAIRO_CONTENT_COLOR_ALPHA;
			break;

		default:
			surface->content = CAIRO_CONTENT_COLOR;
			break;
	}

	_cairo_surface_init(&surface->base, &cairo_amigaos_surface_backend, NULL, surface->content);

	return &surface->base;
}

