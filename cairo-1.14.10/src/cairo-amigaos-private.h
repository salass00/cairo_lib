#ifndef CAIRO_AMIGAOS_PRIVATE_H
#define CAIRO_AMIGAOS_PRIVATE_H

#include "cairo-amigaos.h"

#include "cairoint.h"

typedef struct _cairo_amigaos_surface {
	cairo_surface_t base;

	struct RastPort       *rastport;
	struct BitMap         *bitmap;
	BOOL                   free_rastport:1;
	BOOL                   free_bitmap:1;

	cairo_content_t        content;

	int                    xoff, yoff;
	int                    width, height;

	cairo_image_surface_t *map_image;
	cairo_rectangle_int_t  map_rect;
} cairo_amigaos_surface_t;

#endif /* CAIRO_AMIGAOS_PRIVATE_H */
