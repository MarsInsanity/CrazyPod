#ifndef CRAZYPOD_MONO_H
#define CRAZYPOD_MONO_H

#include "config.h"

#include <stdbool.h>
#include <stdint.h>

#include "crazypod_pixel.h"

/*
 * The monochrome build of the product UI.
 *
 * Two separate jobs live here, and they must not be confused with each other.
 *
 * The first is faithful: turning a finished RGB565 frame into the two bits
 * per pixel the iPod Mini's panel takes. A photograph has to come out of that
 * looking like the photograph, so this step only quantises brightness. It
 * never inverts.
 *
 * The second is a design decision: the product UI was drawn as light type on
 * near-black glass, and that is the wrong way round for a transflective
 * grey panel, which is at its most readable showing dark ink on its own pale
 * background and spends the least backlight doing it. So the UI's own colours
 * are remapped to ink and paper before they are ever drawn, which is where
 * the inversion happens -- to the design's palette, not to its content.
 */

/* The four shades the panel can show, as 0xRRGGBB, darkest first. */
#define CRAZYPOD_MONO_INK        0x000000u
#define CRAZYPOD_MONO_SHADE_DARK 0x555555u
#define CRAZYPOD_MONO_SHADE_PALE 0xAAAAAAu
#define CRAZYPOD_MONO_PAPER      0xFFFFFFu

/* The same four as a level index, 0 = ink, 3 = paper. */
enum crazypod_mono_level {
    CRAZYPOD_MONO_LEVEL_INK = 0,
    CRAZYPOD_MONO_LEVEL_DARK,
    CRAZYPOD_MONO_LEVEL_PALE,
    CRAZYPOD_MONO_LEVEL_PAPER,
    CRAZYPOD_MONO_LEVEL_COUNT
};

/* 0xRRGGBB for a level. */
uint32_t crazypod_mono_level_rgb(unsigned level);

/*
 * Quantise a brightness, 0..255, to one of the four shades. Faithful: the
 * brightest input comes back as the brightest shade.
 */
unsigned crazypod_mono_level_from_luma(unsigned luma);

/*
 * The shade a design colour is drawn as. This is the inverting map: the
 * design's darkest surfaces become paper and its brightest type becomes ink.
 * Returns 0xRRGGBB so it can be handed straight to any call that takes a
 * design colour.
 */
uint32_t crazypod_mono_rgb(uint32_t rgb);

/*
 * The same map, but told how opaque the fill is going to be. An overlay the
 * design draws at a low alpha over its dark backdrop reads as a faint
 * lightening; drawn opaque at its own colour it would read as a solid block.
 * Returns the shade to use once the caller has raised the fill to full
 * opacity, which is what the monochrome build does with every fill.
 */
uint32_t crazypod_mono_rgb_at_opacity(uint32_t rgb, unsigned opacity);

#ifdef HAVE_CRAZYPOD_MONO_UI

/*
 * Write one row of a finished RGB565 strip into the packed 2bpp framebuffer.
 *
 * `row` addresses the first byte of the destination row, `x` is the pixel
 * column the strip starts at, and the pixels are quantised, not inverted.
 */
void crazypod_mono_blit_row(uint8_t *row, int x, int width,
                            const crazypod_pixel_t *source);

/* Set `width` pixels of one row to a hardware value, 0..3, where 3 is the
 * darkest -- the panel's own convention, the inverse of a brightness. */
void crazypod_mono_fill_row(uint8_t *row, int x, int width, unsigned value);

/* The hardware value for a brightness level, 0..3. */
static inline unsigned crazypod_mono_hardware_value(unsigned level)
{
    return (~level) & 3u;
}

#endif /* HAVE_CRAZYPOD_MONO_UI */

#endif
