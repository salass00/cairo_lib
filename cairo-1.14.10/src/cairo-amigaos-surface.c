#include "cairoint.h"

#include "cairo-amigaos.h"
#include "cairo-default-context-private.h"
#include "cairo-surface-fallback-private.h"

#include <proto/graphics.h>

typedef struct _cairo_amigaos_surface {
	cairo_surface_t base;

	struct RastPort *rastport;
	struct BitMap   *bitmap;
	BOOL             free_rastport:1;
	BOOL             free_bitmap:1;

	int              xoff, yoff;
	int              width, height;
} cairo_amigaos_surface_t;

static cairo_status_t
_cairo_amigaos_surface_finish (void *abstract_surface)
{
	cairo_amigaos_surface_t *surface = abstract_surface;

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
	}

	bitmap = IGraphics->AllocBitMapTags(width, height, depth,
		BMATags_Friend,      src->bitmap,
		BMATags_PixelFormat, pixfmt,
		TAG_END);

	surface = (cairo_amigaos_surface_t *)cairo_amigaos_surface_create(bitmap);
	surface->free_bitmap = TRUE;

	return &surface->base;
}

static const cairo_surface_backend_t cairo_amigaos_surface_backend = {
	CAIRO_SURFACE_TYPE_AMIGAOS,
	_cairo_amigaos_surface_finish,
	_cairo_default_context_create,
	_cairo_amigaos_surface_create_similar,
	_cairo_amigaos_surface_create_similar_image,
	_cairo_amigaos_surface_map_to_image,
	_cairo_amigaos_surface_unmap_image,
	_cairo_surface_default_source,
    _cairo_surface_default_acquire_source_image,
    _cairo_surface_default_release_source_image,
	NULL, /* snapshot */
	NULL, /* copy_page */
	NULL, /* show_page */
	_cairo_amigaos_surface_get_extents,
	NULL, /* get_font_options */
	NULL, /* flush */
	NULL, /* mark_dirty_rectangle */
	_cairo_surface_fallback_paint,
	_cairo_surface_fallback_mask,
	_cairo_surface_fallback_stroke,
	_cairo_surface_fallback_fill,
	NULL,  /* fill/stroke */
	_cairo_surface_fallback_glyphs,
	FALSE,
	NULL, /* show_text_glyphs */
	NULL, /* get_supported_mime_types */
};

cairo_surface_t *
cairo_amigaos_surface_create (struct BitMap *bitmap)
{
	cairo_amigaos_surface_t *surface;
	int                      width, height;
	struct RastPort         *rastport;

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
	cairo_content_t          content;

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
		case PIXF_A8R8G8B8:
		case PIXF_R8G8B8A8:
		case PIXF_B8G8R8A8:
		case PIXF_A8B8G8R8:
			content = CAIRO_CONTENT_COLOR_ALPHA;
			break;

		case PIXF_ALPHA8:
		case PIXF_CLUT:
			content = CAIRO_CONTENT_ALPHA;
			break;

		default:
			content = CAIRO_CONTENT_COLOR;
			break;
	}

	_cairo_surface_init(&surface->base, &cairo_amigaos_surface_backend, NULL, content);

	return &surface->base;
}

