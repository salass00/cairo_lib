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

static cairo_device_t *__cairo_amigaos_device;

static cairo_status_t
_cairo_amigaos_device_flush (void *abstract_device)
{
	return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_amigaos_device_finish (void *abstract_device)
{
}

static void
_cairo_amigaos_device_destroy (void *abstract_device)
{
	cairo_amigaos_device_t *device = abstract_device;

	free(device);
}

static const cairo_device_backend_t _cairo_amigaos_device_backend = {
	.type    = CAIRO_DEVICE_TYPE_AMIGAOS,
	.lock    = NULL,
	.unlock  = NULL,
	.flush   = _cairo_amigaos_device_flush,
	.finish  = _cairo_amigaos_device_finish,
	.destroy = _cairo_amigaos_device_destroy
};

cairo_device_t *
_cairo_amigaos_device_get (void)
{
	cairo_amigaos_device_t *device;

	if (__cairo_amigaos_device != NULL)
		return cairo_device_reference(__cairo_amigaos_device);

	device = malloc(sizeof(*device));

	_cairo_device_init(&device->base, &_cairo_amigaos_device_backend);

	device->compositor = _cairo_amigaos_compositor_get();

	if (_cairo_atomic_ptr_cmpxchg((void **)&__cairo_amigaos_device, NULL, device))
		return cairo_device_reference(&device->base);

	_cairo_amigaos_device_destroy(device);
	return cairo_device_reference(__cairo_amigaos_device);
}

