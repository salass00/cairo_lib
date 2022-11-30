#!/bin/sh
#
# Script for creating a release archive
#

CAIROVER='1.14.10'
CAIRODIR="cairo-${CAIROVER}"
DESTDIR='tmp'
NUMVERS=${CAIROVER}

ARCHIVE="cairo_lib-${NUMVERS}.7z"

rm -rf ${DESTDIR}
mkdir -p ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
mkdir -p ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib/pkgconfig

cp -p ${CAIRODIR}/COPYING ${DESTDIR}/cairo_lib-${NUMVERS}
cp -p ${CAIRODIR}/COPYING-LGPL-2.1 ${DESTDIR}/cairo_lib-${NUMVERS}
cp -p ${CAIRODIR}/COPYING-MPL-1.1 ${DESTDIR}/cairo_lib-${NUMVERS}
cp -p ${CAIRODIR}/src/cairo.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p ${CAIRODIR}/src/cairo-amigaos.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p ${CAIRODIR}/src/cairo-deprecated.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p ${CAIRODIR}/src/cairo-features.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p ${CAIRODIR}/src/cairo-ft.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p ${CAIRODIR}/src/cairo-pdf.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p ${CAIRODIR}/src/cairo-ps.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p ${CAIRODIR}/src/cairo-script.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p ${CAIRODIR}/src/cairo-svg.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p ${CAIRODIR}/util/cairo-script/cairo-script-interpreter.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p ${CAIRODIR}/cairo-version.h ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/include/cairo
cp -p cairo-build/src/.libs/libcairo.a ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib
cp -p cairo-build/src/libcairo.la ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib
cp -p cairo-build/src/cairo.pc ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib/pkgconfig
cp -p cairo-build/src/cairo-amigaos.pc ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib/pkgconfig
cp -p cairo-build/src/cairo-ft.pc ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib/pkgconfig
cp -p cairo-build/src/cairo-pdf.pc ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib/pkgconfig
cp -p cairo-build/src/cairo-png.pc ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib/pkgconfig
cp -p cairo-build/src/cairo-ps.pc ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib/pkgconfig
cp -p cairo-build/src/cairo-script.pc ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib/pkgconfig
cp -p cairo-build/src/cairo-svg.pc ${DESTDIR}/cairo_lib-${NUMVERS}/SDK/local/newlib/lib/pkgconfig

echo "Short:        Cairo - 2D graphics library"          > ${DESTDIR}/cairo_lib-${NUMVERS}/cairo_lib.readme
echo "Author:       Fredrik Wikstrom (AmigaOS port)"     >> ${DESTDIR}/cairo_lib-${NUMVERS}/cairo_lib.readme
echo "Uploader:     Fredrik Wikstrom <fredrik@a500.org>" >> ${DESTDIR}/cairo_lib-${NUMVERS}/cairo_lib.readme
echo "Type:         dev/lib"                             >> ${DESTDIR}/cairo_lib-${NUMVERS}/cairo_lib.readme
echo "Version:      ${NUMVERS}"                          >> ${DESTDIR}/cairo_lib-${NUMVERS}/cairo_lib.readme
echo "Architecture: ppc-amigaos"                         >> ${DESTDIR}/cairo_lib-${NUMVERS}/cairo_lib.readme
echo ""                                                  >> ${DESTDIR}/cairo_lib-${NUMVERS}/cairo_lib.readme
cat README                                               >> ${DESTDIR}/cairo_lib-${NUMVERS}/cairo_lib.readme

rm -f ${ARCHIVE}
7za u ${ARCHIVE} ./${DESTDIR}/*

rm -rf ${DESTDIR}

echo "${ARCHIVE} created"

