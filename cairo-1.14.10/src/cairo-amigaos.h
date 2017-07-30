#ifndef _CAIRO_AMIGAOS_H_
#define _CAIRO_AMIGAOS_H_

#include "cairo.h"

#if CAIRO_HAS_AMIGAOS_SURFACE

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

#else /* CAIRO_HAS_AMIGAOS_SURFACE */
# error Cairo was not compiled with support for the AmigaOS backend
#endif /* CAIRO_HAS_AMIGAOS_SURFACE */

#endif /* _CAIRO_AMIGAOS_H_ */

