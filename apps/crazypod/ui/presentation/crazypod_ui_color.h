#ifndef CRAZYPOD_UI_COLOR_H
#define CRAZYPOD_UI_COLOR_H

#include "config.h"

#include <stdint.h>

#include "lvgl.h"

#include "../../crazypod_mono.h"

/*
 * Every colour the product UI draws with goes through here.
 *
 * On the colour iPods this is lv_color_hex and nothing more. On the Mini it
 * is where the design's palette is remapped to the four shades the panel can
 * show -- inverted, so the near-black page becomes paper and white type
 * becomes ink. Doing it at the one place the UI names a colour means the
 * design keeps reading in its own terms everywhere else, and a screen nobody
 * has redrawn for the small panel still comes out legible rather than as two
 * indistinguishable blacks.
 *
 * Opacities are deliberately left alone. A scrim the design draws as white
 * at a fifth opacity over its dark page lightens it; mapped to ink at the
 * same fifth over paper, it darkens paper by about as much. The inversion
 * carries through the blend on its own.
 */
static inline lv_color_t crazypod_ui_color(uint32_t rgb)
{
#ifdef HAVE_CRAZYPOD_MONO_UI
    rgb = crazypod_mono_rgb(rgb);
#endif
    return lv_color_hex(rgb);
}

/* The same map as a plain 0xRRGGBB, for the calls that carry one. */
static inline uint32_t crazypod_ui_rgb(uint32_t rgb)
{
#ifdef HAVE_CRAZYPOD_MONO_UI
    return crazypod_mono_rgb(rgb);
#else
    return rgb;
#endif
}

#endif
