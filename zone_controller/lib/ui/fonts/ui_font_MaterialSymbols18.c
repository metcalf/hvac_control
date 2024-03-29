/*******************************************************************************
 * Size: 18 px
 * Bpp: 2
 * Opts: --bpp 2 --size 18 --font /Users/andrew/code/hvac_control/zone_controller/squareline/assets/MaterialSymbolsOutlined.ttf -o /Users/andrew/code/hvac_control/zone_controller/squareline/assets/ui_font_MaterialSymbols18.c --format lvgl -r 0xe88a -r 0xe5cd -r 0xe5c9 -r 0xe14b -r 0xe5ca -r 0xe86c -r 0xe406 -r 0xf16a -r 0xf537 -r 0xf16b -r 0xf557 -r 0xf166 -r 0xf16d -r 0xf167 -r 0xf168 -r 0xec17 -r 0xe719 -r 0xefd8 -r 0xe836 -r 0xe648 -r 0xe5c4 -r 0xe5c8 -r 0xe8b8 -r 0xe145 -r 0xf164 -r 0xec18 --no-compress --no-prefilter --no-kerning
 ******************************************************************************/

#include "../ui.h"

#ifndef UI_FONT_MATERIALSYMBOLS18
#define UI_FONT_MATERIALSYMBOLS18 1
#endif

#if UI_FONT_MATERIALSYMBOLS18

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+E145 "" */
    0x0, 0x14, 0x0, 0x0, 0x2c, 0x0, 0x0, 0x2c,
    0x0, 0x0, 0x2c, 0x0, 0x0, 0x2c, 0x0, 0x3f,
    0xff, 0xfd, 0x2a, 0xbe, 0xa8, 0x0, 0x2c, 0x0,
    0x0, 0x2c, 0x0, 0x0, 0x2c, 0x0, 0x0, 0x2c,
    0x0, 0x0, 0x0, 0x0,

    /* U+E14B "" */
    0x0, 0x5, 0x50, 0x0, 0x0, 0xbf, 0xfe, 0x0,
    0x3, 0xe0, 0xb, 0xc0, 0xf, 0x80, 0x0, 0xf0,
    0x2e, 0xe0, 0x0, 0x38, 0x38, 0xb8, 0x0, 0x2c,
    0x70, 0x2e, 0x0, 0xd, 0x70, 0xb, 0x80, 0xd,
    0x70, 0x2, 0xe0, 0xd, 0x70, 0x0, 0xb8, 0xd,
    0x38, 0x0, 0x2e, 0x2c, 0x2c, 0x0, 0xb, 0xb8,
    0xf, 0x0, 0x2, 0xf0, 0x3, 0xe0, 0xb, 0xc0,
    0x0, 0xbf, 0xfe, 0x0, 0x0, 0x5, 0x50, 0x0,

    /* U+E406 "" */
    0x0, 0x14, 0x0, 0x2, 0xff, 0x80, 0x7, 0x82,
    0xd0, 0xb, 0x0, 0xe0, 0x2e, 0x0, 0xb8, 0xb4,
    0x0, 0x1e, 0xe0, 0x0, 0xb, 0xe0, 0x0, 0xb,
    0xf0, 0x0, 0xf, 0x7e, 0xaa, 0xbc, 0x1f, 0xff,
    0xf0, 0x0, 0x38, 0x0, 0x0, 0x38, 0x0, 0x0,
    0x38, 0x0, 0x7f, 0xff, 0xfd, 0x2a, 0xaa, 0xa8,

    /* U+E5C4 "" */
    0x0, 0x14, 0x0, 0x0, 0x7c, 0x0, 0x1, 0xf0,
    0x0, 0x7, 0xc0, 0x0, 0x1f, 0x0, 0x0, 0x7e,
    0xaa, 0xaa, 0xbf, 0xff, 0xff, 0x2d, 0x0, 0x0,
    0xb, 0x40, 0x0, 0x2, 0xd0, 0x0, 0x0, 0xb4,
    0x0, 0x0, 0x28, 0x0, 0x0, 0x0, 0x0,

    /* U+E5C8 "" */
    0x0, 0x14, 0x0, 0x0, 0x3d, 0x0, 0x0, 0xf,
    0x40, 0x0, 0x3, 0xd0, 0x0, 0x0, 0xf4, 0xaa,
    0xaa, 0xbd, 0xff, 0xff, 0xfe, 0x0, 0x0, 0x78,
    0x0, 0x1, 0xe0, 0x0, 0x7, 0x80, 0x0, 0x1e,
    0x0, 0x0, 0x28, 0x0, 0x0, 0x0, 0x0,

    /* U+E5C9 "" */
    0x0, 0x5, 0x50, 0x0, 0x0, 0xbf, 0xfe, 0x0,
    0x3, 0xe0, 0xb, 0xc0, 0xf, 0x0, 0x0, 0xf0,
    0x2c, 0x10, 0x4, 0x38, 0x38, 0x78, 0x2d, 0x2c,
    0x70, 0x2e, 0xb8, 0xd, 0x70, 0xb, 0xe0, 0xd,
    0x70, 0xb, 0xe0, 0xd, 0x70, 0x2e, 0xb8, 0xd,
    0x38, 0x78, 0x2d, 0x2c, 0x2c, 0x10, 0x4, 0x38,
    0xf, 0x0, 0x0, 0xf0, 0x3, 0xe0, 0xb, 0xc0,
    0x0, 0xbf, 0xfe, 0x0, 0x0, 0x5, 0x50, 0x0,

    /* U+E5CA "" */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3c, 0x0,
    0x0, 0xf, 0x0, 0x0, 0x3, 0xc0, 0x14, 0x0,
    0xf0, 0x3, 0xd0, 0x3c, 0x0, 0xf, 0x4f, 0x0,
    0x0, 0x3f, 0xc0, 0x0, 0x0, 0xf0, 0x0, 0x0,
    0x0, 0x0, 0x0,

    /* U+E5CD "" */
    0x0, 0x0, 0x0, 0x38, 0x0, 0x2c, 0x2e, 0x0,
    0xb8, 0xb, 0x82, 0xe0, 0x2, 0xeb, 0x80, 0x0,
    0xbe, 0x0, 0x0, 0xbe, 0x0, 0x2, 0xeb, 0x80,
    0xb, 0x82, 0xe0, 0x2e, 0x0, 0xb8, 0x38, 0x0,
    0x2c, 0x0, 0x0, 0x0,

    /* U+E648 "" */
    0x14, 0x0, 0x0, 0x0, 0x2, 0xe0, 0xbf, 0xf8,
    0x0, 0xb, 0x87, 0xff, 0xfc, 0x2, 0xfe, 0x5,
    0x6f, 0xf8, 0xbf, 0xb8, 0x0, 0x1f, 0xe7, 0x82,
    0xe0, 0x40, 0x2d, 0x0, 0x2f, 0x87, 0x80, 0x0,
    0x1f, 0xfe, 0x1f, 0x40, 0x1, 0xf4, 0xb8, 0x74,
    0x0, 0x0, 0x2, 0xe0, 0x0, 0x0, 0x1, 0xff,
    0x80, 0x0, 0x0, 0x3f, 0xee, 0x0, 0x0, 0x2,
    0xf8, 0xb8, 0x0, 0x0, 0xa, 0x2, 0xd0, 0x0,
    0x0, 0x0, 0x9, 0x0,

    /* U+E719 "" */
    0x0, 0x5, 0x50, 0x0, 0x0, 0xbf, 0xfe, 0x0,
    0x3, 0xe0, 0xb, 0xc0, 0xf, 0x1, 0x40, 0xf0,
    0x2c, 0x2f, 0xf8, 0x38, 0x38, 0xb4, 0x1e, 0x2c,
    0x70, 0xd0, 0x7, 0xd, 0x71, 0xc7, 0xc3, 0x4d,
    0x71, 0xcb, 0xc3, 0x4d, 0x70, 0xd2, 0x47, 0xd,
    0x38, 0xb4, 0x1e, 0x2c, 0x2c, 0x2f, 0xf8, 0x38,
    0xf, 0x1, 0x40, 0xf0, 0x3, 0xe0, 0xb, 0xc0,
    0x0, 0xbf, 0xfe, 0x0, 0x0, 0x5, 0x50, 0x0,

    /* U+E836 "" */
    0x0, 0x5, 0x50, 0x0, 0x0, 0xbf, 0xfe, 0x0,
    0x3, 0xe0, 0xb, 0xc0, 0xf, 0x0, 0x0, 0xf0,
    0x2c, 0x0, 0x0, 0x38, 0x38, 0x0, 0x0, 0x2c,
    0x70, 0x0, 0x0, 0xd, 0x70, 0x0, 0x0, 0xd,
    0x70, 0x0, 0x0, 0xd, 0x70, 0x0, 0x0, 0xd,
    0x38, 0x0, 0x0, 0x2c, 0x2c, 0x0, 0x0, 0x38,
    0xf, 0x0, 0x0, 0xf0, 0x3, 0xe0, 0xb, 0xc0,
    0x0, 0xbf, 0xfe, 0x0, 0x0, 0x5, 0x50, 0x0,

    /* U+E86C "" */
    0x0, 0x5, 0x50, 0x0, 0x0, 0xbf, 0xfe, 0x0,
    0x3, 0xe0, 0xb, 0xc0, 0xf, 0x0, 0x0, 0xf0,
    0x2c, 0x0, 0x0, 0x38, 0x38, 0x0, 0x2, 0x2c,
    0x70, 0x0, 0xf, 0xd, 0x70, 0x40, 0x3c, 0xd,
    0x70, 0xf0, 0xf0, 0xd, 0x70, 0x3f, 0xc0, 0xd,
    0x38, 0xf, 0x0, 0x2c, 0x2c, 0x0, 0x0, 0x38,
    0xf, 0x0, 0x0, 0xf0, 0x3, 0xe0, 0xb, 0xc0,
    0x0, 0xbf, 0xfe, 0x0, 0x0, 0x5, 0x50, 0x0,

    /* U+E88A "" */
    0x0, 0x18, 0x0, 0x0, 0xbf, 0x0, 0x7, 0xd3,
    0xd0, 0x2f, 0x0, 0xb8, 0xf8, 0x0, 0x2f, 0xe0,
    0x0, 0xb, 0xe0, 0x0, 0xb, 0xe0, 0x55, 0xb,
    0xe0, 0xff, 0x4b, 0xe0, 0xd3, 0x4b, 0xe0, 0xd3,
    0x4b, 0xe0, 0xd3, 0x4b, 0xfa, 0xd3, 0xaf, 0xff,
    0xd3, 0xff,

    /* U+E8B8 "" */
    0x0, 0xa, 0xa0, 0x0, 0x0, 0xf, 0xf0, 0x0,
    0x4, 0x1c, 0x34, 0x10, 0xf, 0xbc, 0x3e, 0xf0,
    0x2e, 0xe0, 0xb, 0xb8, 0x38, 0x2, 0x80, 0x2c,
    0x3e, 0xf, 0xf0, 0xbc, 0xf, 0x2f, 0xf8, 0xf0,
    0xf, 0x2f, 0xf8, 0xe0, 0x3e, 0xf, 0xf0, 0xbc,
    0x38, 0x2, 0x80, 0x2c, 0x2e, 0xe0, 0xb, 0xb8,
    0xf, 0xbc, 0x3e, 0xf0, 0x4, 0x1c, 0x34, 0x10,
    0x0, 0xf, 0xf0, 0x0, 0x0, 0xa, 0xa0, 0x0,

    /* U+EC17 "" */
    0x0, 0x0, 0x54, 0x0, 0xe, 0x0, 0xff, 0xc0,
    0x2, 0xe0, 0xf4, 0xb0, 0x0, 0x2e, 0x18, 0x78,
    0x0, 0x2, 0xe0, 0x38, 0x0, 0x3, 0xfe, 0xd,
    0x7f, 0x1, 0xef, 0xe1, 0xfe, 0xf0, 0x71, 0xfe,
    0x18, 0x1d, 0x1d, 0xb, 0xe1, 0xd3, 0x43, 0xef,
    0xfe, 0x1e, 0xd0, 0x3f, 0x5f, 0xe1, 0xe0, 0x0,
    0xb, 0x2e, 0x0, 0x0, 0xb, 0x4b, 0xe0, 0x0,
    0x3, 0x87, 0xee, 0x0, 0x0, 0xff, 0xc2, 0xe0,
    0x0, 0x5, 0x40, 0x2c, 0x0, 0x0, 0x0, 0x0,
    0x0,

    /* U+EC18 "" */
    0x2a, 0xaa, 0xaa, 0x4b, 0xff, 0xff, 0xfd, 0xf0,
    0x4, 0x0, 0xef, 0xb, 0xfd, 0xe, 0xf2, 0xee,
    0xf4, 0xef, 0x3f, 0xef, 0xce, 0xf7, 0x7b, 0xad,
    0xef, 0xbf, 0x2f, 0xde, 0xf7, 0x7f, 0xec, 0xef,
    0x3d, 0xdf, 0x8e, 0xf0, 0xff, 0xf0, 0xef, 0x1,
    0xa4, 0xe, 0xba, 0xaa, 0xaa, 0xe7, 0xff, 0xff,
    0xfc,

    /* U+EFD8 "" */
    0x0, 0x0, 0x19, 0x0, 0x0, 0x0, 0xbf, 0xc0,
    0x0, 0x0, 0xe1, 0xc0, 0x0, 0x0, 0x1, 0xc0,
    0xbf, 0xff, 0xff, 0x80, 0x6a, 0xaa, 0xa5, 0x0,
    0x7f, 0xff, 0xff, 0xe0, 0x7f, 0xff, 0xff, 0xfc,
    0x6a, 0xa9, 0x40, 0xd, 0xbf, 0xff, 0xd0, 0x1d,
    0x0, 0x0, 0xf0, 0x3c, 0x0, 0x1e, 0xe0, 0x10,
    0x0, 0xb, 0xc0, 0x0,

    /* U+F164 "" */
    0x0, 0x0, 0x0, 0x0, 0x28, 0x0, 0x0, 0xbe,
    0x0, 0x2, 0xd7, 0x80, 0xb, 0x41, 0xe0, 0x2d,
    0x0, 0x78, 0x74, 0x0, 0x1d, 0xb0, 0x0, 0xe,
    0xe0, 0x0, 0xb, 0xe0, 0x0, 0xb, 0xe0, 0x0,
    0xb, 0xb0, 0x0, 0xe, 0x3c, 0x0, 0x3c, 0xf,
    0x96, 0xf0, 0x2, 0xff, 0x80, 0x0, 0x0, 0x0,

    /* U+F166 "" */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x80, 0x0,
    0x0, 0xf3, 0xcf, 0x0, 0x0, 0x3f, 0xfc, 0x0,
    0x1c, 0xf, 0xf0, 0x30, 0x1f, 0x3, 0xc0, 0xf0,
    0x7, 0xc3, 0xc3, 0xc0, 0x7f, 0xff, 0xff, 0xf8,
    0x7f, 0xff, 0xff, 0xf8, 0x7, 0xc3, 0xc3, 0xc0,
    0x1f, 0x3, 0xc0, 0xf0, 0x1c, 0xf, 0xf0, 0x30,
    0x0, 0x3f, 0xfc, 0x0, 0x0, 0xf7, 0xdf, 0x0,
    0x0, 0x53, 0xc5, 0x0, 0x0, 0x1, 0x40, 0x0,

    /* U+F167 "" */
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x28, 0x0,
    0x1, 0x3, 0xcf, 0x3c, 0x0, 0xf0, 0x3f, 0xfc,
    0x0, 0xf, 0x3, 0xfc, 0xc, 0x0, 0xf0, 0x3c,
    0xf, 0x0, 0xf, 0xb, 0xf, 0x0, 0x7f, 0xf0,
    0xbf, 0xf8, 0x1f, 0xff, 0xb, 0xfe, 0x0, 0x7d,
    0xf0, 0x3c, 0x0, 0x7c, 0x1f, 0x3, 0xc0, 0x1c,
    0xf, 0xf0, 0x30, 0x0, 0xf, 0xff, 0x0, 0x0,
    0xf, 0x7c, 0xf0, 0x0, 0x1, 0x4f, 0xf, 0x0,
    0x0, 0x1, 0x40, 0xf0, 0x0, 0x0, 0x0, 0x8,
    0x0,

    /* U+F168 "" */
    0x0, 0x1, 0x50, 0x0, 0x0, 0xf, 0xfc, 0x0,
    0x0, 0x3d, 0x2c, 0x0, 0x0, 0x38, 0x78, 0x0,
    0x0, 0x3c, 0xe0, 0x0, 0x3e, 0x1e, 0xd7, 0xf0,
    0x7b, 0x9f, 0xff, 0xbc, 0x71, 0xfc, 0x38, 0x1d,
    0x74, 0x2c, 0x3f, 0x4d, 0x3e, 0xff, 0xf6, 0xed,
    0xf, 0xd7, 0xb4, 0xb8, 0x0, 0xb, 0x3c, 0x0,
    0x0, 0x2d, 0x2c, 0x0, 0x0, 0x38, 0x7c, 0x0,
    0x0, 0x3f, 0xf0, 0x0, 0x0, 0x5, 0x40, 0x0,

    /* U+F16A "" */
    0x0, 0x20, 0x0, 0x0, 0xf0, 0x0, 0x3, 0xf0,
    0x0, 0xf, 0x74, 0x50, 0x3c, 0x3f, 0xf8, 0x74,
    0x6, 0x6c, 0xb0, 0x0, 0xe, 0xe0, 0x0, 0xb,
    0xe0, 0x7d, 0xb, 0xe0, 0xff, 0xb, 0xb3, 0xc3,
    0xce, 0x3b, 0x82, 0xec, 0x1f, 0xc3, 0xf4, 0x7,
    0xff, 0xd0, 0x0, 0x55, 0x0,

    /* U+F16B "" */
    0x0, 0x40, 0x0, 0x0, 0x78, 0x0, 0x0, 0xf,
    0xc0, 0x0, 0x3, 0xdf, 0xf0, 0x8, 0x70, 0x6b,
    0x83, 0xdb, 0x0, 0x20, 0xf4, 0xf1, 0xe0, 0x2d,
    0xb, 0x3f, 0xb, 0xfe, 0x7f, 0x2, 0xea, 0x92,
    0xf0, 0xbe, 0x0, 0x5, 0x2e, 0x7f, 0xe0, 0xb,
    0xe3, 0xf9, 0x2, 0xde, 0x3f, 0x80, 0x74, 0xe3,
    0x98, 0x0, 0x5, 0x14, 0x0,

    /* U+F16D "" */
    0x0, 0x0, 0x0, 0x1, 0x40, 0x2c, 0x0, 0xb,
    0x83, 0xf0, 0x0, 0xb, 0x86, 0xc0, 0x0, 0xb,
    0x83, 0xef, 0x40, 0xf, 0x83, 0xff, 0x0, 0xbb,
    0x80, 0xe, 0x3, 0x8b, 0x80, 0x2c, 0xe, 0xb,
    0x80, 0xb0, 0x38, 0x3f, 0x82, 0xc0, 0xf2, 0xdb,
    0x86, 0x1, 0xde, 0xb, 0x80, 0x3, 0xf8, 0x2f,
    0x80, 0x2, 0xff, 0xff, 0x80, 0x1, 0xbe, 0x4b,
    0x40, 0x0, 0x0, 0x9,

    /* U+F537 "" */
    0x24, 0x20, 0x60, 0x2c, 0x3c, 0x78, 0xe, 0x1d,
    0x2c, 0xb, 0xe, 0x1c, 0xb, 0xe, 0x1c, 0xe,
    0x1d, 0x2c, 0x1d, 0x2c, 0x38, 0x2c, 0x38, 0x70,
    0x38, 0x70, 0xb0, 0x34, 0xb0, 0xe0, 0x34, 0xb0,
    0xe0, 0x38, 0x74, 0xb0, 0x2d, 0x3c, 0x38, 0x9,
    0x8, 0x18,

    /* U+F557 "" */
    0x0, 0x0, 0x0, 0x0, 0x4, 0x10, 0x34, 0x0,
    0x3, 0xcf, 0xe, 0x78, 0x0, 0x38, 0xe3, 0xfc,
    0x0, 0xb, 0x2c, 0xf8, 0x1c, 0x2, 0xcb, 0x3c,
    0x1f, 0x0, 0xe3, 0x8e, 0x1f, 0x0, 0x34, 0xd3,
    0xff, 0xf8, 0x2c, 0xb0, 0xff, 0xfe, 0xe, 0x38,
    0x38, 0x7c, 0x3, 0x4d, 0xe, 0x7, 0xc0, 0xd3,
    0x43, 0xe0, 0x70, 0x38, 0xe0, 0xfe, 0x0, 0xb,
    0x2c, 0x39, 0xe0, 0x0, 0xd3, 0x4e, 0x14, 0x0,
    0x0, 0x2, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 288, .box_w = 12, .box_h = 12, .ofs_x = 3, .ofs_y = 3},
    {.bitmap_index = 36, .adv_w = 288, .box_w = 16, .box_h = 16, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 100, .adv_w = 288, .box_w = 12, .box_h = 16, .ofs_x = 3, .ofs_y = 1},
    {.bitmap_index = 148, .adv_w = 288, .box_w = 12, .box_h = 13, .ofs_x = 3, .ofs_y = 2},
    {.bitmap_index = 187, .adv_w = 288, .box_w = 12, .box_h = 13, .ofs_x = 3, .ofs_y = 2},
    {.bitmap_index = 226, .adv_w = 288, .box_w = 16, .box_h = 16, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 290, .adv_w = 288, .box_w = 14, .box_h = 10, .ofs_x = 2, .ofs_y = 4},
    {.bitmap_index = 325, .adv_w = 288, .box_w = 12, .box_h = 12, .ofs_x = 3, .ofs_y = 3},
    {.bitmap_index = 361, .adv_w = 288, .box_w = 18, .box_h = 15, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 429, .adv_w = 288, .box_w = 16, .box_h = 16, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 493, .adv_w = 288, .box_w = 16, .box_h = 16, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 557, .adv_w = 288, .box_w = 16, .box_h = 16, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 621, .adv_w = 288, .box_w = 12, .box_h = 14, .ofs_x = 3, .ofs_y = 2},
    {.bitmap_index = 663, .adv_w = 288, .box_w = 16, .box_h = 16, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 727, .adv_w = 288, .box_w = 17, .box_h = 17, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 800, .adv_w = 288, .box_w = 14, .box_h = 14, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 849, .adv_w = 288, .box_w = 16, .box_h = 13, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 901, .adv_w = 288, .box_w = 12, .box_h = 16, .ofs_x = 3, .ofs_y = 1},
    {.bitmap_index = 949, .adv_w = 288, .box_w = 16, .box_h = 16, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 1013, .adv_w = 288, .box_w = 17, .box_h = 17, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1086, .adv_w = 288, .box_w = 16, .box_h = 16, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 1150, .adv_w = 288, .box_w = 12, .box_h = 15, .ofs_x = 3, .ofs_y = 1},
    {.bitmap_index = 1195, .adv_w = 288, .box_w = 14, .box_h = 15, .ofs_x = 2, .ofs_y = 1},
    {.bitmap_index = 1248, .adv_w = 288, .box_w = 15, .box_h = 16, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 1308, .adv_w = 288, .box_w = 12, .box_h = 14, .ofs_x = 3, .ofs_y = 2},
    {.bitmap_index = 1350, .adv_w = 288, .box_w = 17, .box_h = 16, .ofs_x = 1, .ofs_y = 1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x6, 0x2c1, 0x47f, 0x483, 0x484, 0x485, 0x488,
    0x503, 0x5d4, 0x6f1, 0x727, 0x745, 0x773, 0xad2, 0xad3,
    0xe93, 0x101f, 0x1021, 0x1022, 0x1023, 0x1025, 0x1026, 0x1028,
    0x13f2, 0x1412
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 57669, .range_length = 5139, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 26, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LV_VERSION_CHECK(8, 0, 0)
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 2,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LV_VERSION_CHECK(8, 0, 0)
    .cache = &cache
#endif
};


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LV_VERSION_CHECK(8, 0, 0)
const lv_font_t ui_font_MaterialSymbols18 = {
#else
lv_font_t ui_font_MaterialSymbols18 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 17,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};



#endif /*#if UI_FONT_MATERIALSYMBOLS18*/

