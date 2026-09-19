#ifndef CRAZYPOD_PIXEL_H
#define CRAZYPOD_PIXEL_H

#include <stdint.h>

/*
 * The pixel the product UI composes in.
 *
 * LVGL renders RGB565 on every CrazyPod target, and so does everything that
 * hands it a bitmap: album art, photos, wallpaper, sampled backdrops, video
 * frames. On the colour iPods that is also the panel's own format, so the
 * composed pixel and the framebuffer pixel are the same thing and Rockbox's
 * fb_data would do. The iPod Mini's panel takes two bits per pixel, where
 * fb_data is a byte holding four of them, and nothing the UI composes can be
 * expressed in it. So the UI says what it composes in its own words, and the
 * conversion to the panel's format happens once, where the finished frame is
 * handed over.
 *
 * Deliberately not fb_data even where they would be identical: a decoder
 * writing RGB565 should not have to be read as agreeing with whatever the
 * panel happens to want.
 */
typedef uint16_t crazypod_pixel_t;

/* Pack 8-bit components into RGB565. Matches Rockbox's LCD_RGBPACK on the
 * colour targets, which is what the shared decoders and LVGL both produce. */
#define CRAZYPOD_PIXEL_PACK(r, g, b) \
    ((crazypod_pixel_t)((((unsigned)(r) >> 3) << 11) | \
                        (((unsigned)(g) >> 2) << 5) | \
                        ((unsigned)(b) >> 3)))

/* Pack a 0xRRGGBB design colour. */
#define CRAZYPOD_PIXEL_PACK_HEX(rgb) \
    CRAZYPOD_PIXEL_PACK(((unsigned)(rgb) >> 16) & 0xff, \
                        ((unsigned)(rgb) >> 8) & 0xff, \
                        (unsigned)(rgb) & 0xff)

/* Recover 8-bit components, low bits replicated as Rockbox does. */
#define CRAZYPOD_PIXEL_RED(p) \
    ((((unsigned)(p) >> 8) & 0xf8) | (((unsigned)(p) >> 13) & 0x07))
#define CRAZYPOD_PIXEL_GREEN(p) \
    ((((unsigned)(p) >> 3) & 0xfc) | (((unsigned)(p) >> 9) & 0x03))
#define CRAZYPOD_PIXEL_BLUE(p) \
    ((((unsigned)(p) << 3) & 0xf8) | (((unsigned)(p) >> 2) & 0x07))

/*
 * Rec.601 luma of an RGB565 pixel, 0..255.
 *
 * The weights are the usual 0.299/0.587/0.114 scaled by each field's width,
 * so a full-scale pixel lands on 255 without first expanding the components.
 */
static inline unsigned crazypod_pixel_luma(crazypod_pixel_t pixel)
{
    unsigned red = ((unsigned)pixel >> 11) & 0x1f;
    unsigned green = ((unsigned)pixel >> 5) & 0x3f;
    unsigned blue = (unsigned)pixel & 0x1f;
    unsigned luma = (red * 628u + green * 612u + blue * 236u) >> 8;

    return luma > 255u ? 255u : luma;
}

/*
 * The RGB565 grey for one of the four shades a 2bpp panel can show, 0 =
 * darkest. The chosen values are 0x00/0x55/0xAA/0xFF replicated across the
 * channels, so a pixel written here and quantised back on its way to the
 * panel lands on exactly the shade it was asked for, with no drift.
 */
static inline crazypod_pixel_t crazypod_gray_pixel(unsigned level)
{
    static const crazypod_pixel_t shades[4] = {
        0x0000, 0x52aa, 0xad55, 0xffff
    };

    return shades[level & 3u];
}

/* Rec.601 luma of a 0xRRGGBB design colour, 0..255. */
static inline unsigned crazypod_rgb_luma(uint32_t rgb)
{
    unsigned red = (rgb >> 16) & 0xff;
    unsigned green = (rgb >> 8) & 0xff;
    unsigned blue = rgb & 0xff;

    return (red * 77u + green * 150u + blue * 29u) >> 8;
}

#endif
