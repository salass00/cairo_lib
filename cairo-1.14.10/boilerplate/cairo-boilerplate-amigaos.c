#include "cairo-boilerplate-private.h"

#include <cairo-amigaos.h>

#include <proto/graphics.h>

cairo_surface_t *
_cairo_boilerplate_amigaos_create_surface (const char                *name,
                                           cairo_content_t            content,
                                           int                        width,
                                           int                        height,
                                           cairo_boilerplate_mode_t   mode,
                                           void                     **closure)
{
	cairo_surface_t *surface;
	int              depth;
	uint32           pixfmt;
	struct BitMap   *bitmap;

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
		BMATags_PixelFormat, pixfmt,
		TAG_END);

	surface = (cairo_amigaos_surface_t *)cairo_amigaos_surface_create(bitmap);
	surface->free_bitmap = TRUE;

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
