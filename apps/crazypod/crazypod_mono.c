#include "config.h"

#ifdef HAVE_CRAZYPOD_UI

#include "crazypod_mono.h"

/*
 * Where the design's colours land on the four shades.
 *
 * The product UI is a dark one: its page is 0x08080D, its raised surfaces sit
 * around 0x1B1B22, its secondary type around 0x9A9AA4 and its primary type is
 * white. Read those luminances straight onto a grey panel and the page and
 * the panels both come out black, the whole screen is lit, and nothing can be
 * told from anything. Inverted, the same four bands separate cleanly and the
 * result is the arrangement a reflective panel is good at: dark type on its
 * own pale background.
 *
 * The bands are chosen from where the design's colours actually cluster, not
 * from even quarters of the range, which would put the page and the panels in
 * the same band.
 */
#define DESIGN_PAGE_MAX  20u   /* the near-black page and its siblings */
#define DESIGN_PANEL_MAX 70u   /* raised surfaces, glass, dividers      */
#define DESIGN_MUTED_MAX 190u  /* secondary type, accents, chrome       */

/* Faithful quantiser: midpoints between the four evenly spaced shades. */
#define SHADE_LOW_MAX  42u
#define SHADE_MID_MAX  127u
#define SHADE_HIGH_MAX 212u

uint32_t crazypod_mono_level_rgb(unsigned level)
{
    switch(level) {
    case CRAZYPOD_MONO_LEVEL_INK:
        return CRAZYPOD_MONO_INK;
    case CRAZYPOD_MONO_LEVEL_DARK:
        return CRAZYPOD_MONO_SHADE_DARK;
    case CRAZYPOD_MONO_LEVEL_PALE:
        return CRAZYPOD_MONO_SHADE_PALE;
    default:
        return CRAZYPOD_MONO_PAPER;
    }
}

unsigned crazypod_mono_level_from_luma(unsigned luma)
{
    if(luma <= SHADE_LOW_MAX)
        return CRAZYPOD_MONO_LEVEL_INK;
    if(luma <= SHADE_MID_MAX)
        return CRAZYPOD_MONO_LEVEL_DARK;
    if(luma <= SHADE_HIGH_MAX)
        return CRAZYPOD_MONO_LEVEL_PALE;
    return CRAZYPOD_MONO_LEVEL_PAPER;
}

static unsigned design_level_from_luma(unsigned luma)
{
    if(luma <= DESIGN_PAGE_MAX)
        return CRAZYPOD_MONO_LEVEL_PAPER;
    if(luma <= DESIGN_PANEL_MAX)
        return CRAZYPOD_MONO_LEVEL_PALE;
    if(luma <= DESIGN_MUTED_MAX)
        return CRAZYPOD_MONO_LEVEL_DARK;
    return CRAZYPOD_MONO_LEVEL_INK;
}

uint32_t crazypod_mono_rgb(uint32_t rgb)
{
    return crazypod_mono_level_rgb(
        design_level_from_luma(crazypod_rgb_luma(rgb)));
}

uint32_t crazypod_mono_rgb_at_opacity(uint32_t rgb, unsigned opacity)
{
    unsigned luma = crazypod_rgb_luma(rgb);

    if(opacity >= 255u)
        return crazypod_mono_rgb(rgb);
    /*
     * The design composites this fill over its own near-black page, so its
     * effective brightness is what the alpha lets through. Fold that in
     * before the band lookup and a 20% white scrim stops being white.
     */
    luma = (luma * opacity) / 255u;
    return crazypod_mono_level_rgb(design_level_from_luma(luma));
}

#ifdef HAVE_CRAZYPOD_MONO_UI

/*
 * Packed 2bpp, four pixels to a byte, leftmost pixel in the high bits, and
 * the stored value is the inverse of the brightness: 0b11 is the darkest the
 * panel goes. That is the HD66753's own convention, and it is what Rockbox's
 * own 2bpp driver writes, so a CrazyPod frame and a Rockbox bootloader frame
 * agree about which way up the screen is.
 */
static const uint8_t pixel_mask[4] = { 0xc0, 0x30, 0x0c, 0x03 };

void crazypod_mono_blit_row(uint8_t *row, int x, int width,
                            const crazypod_pixel_t *source)
{
    uint8_t *byte = row + (x >> 2);
    int slot = x & 3;
    int index = 0;

    /*
     * Whole bytes are built in a register and stored once; the partial byte
     * at each end is read back so the pixels outside the strip survive. The
     * byte past the strip is never read, which matters because the last row
     * of a 138-pixel line ends mid-byte at the end of the framebuffer.
     */
    while(index < width) {
        unsigned accumulator;
        int span = 4 - slot;

        if(span > width - index)
            span = width - index;
        accumulator = (slot != 0 || span != 4) ? *byte : 0u;
        while(span-- > 0) {
            unsigned value = crazypod_mono_hardware_value(
                crazypod_mono_level_from_luma(
                    crazypod_pixel_luma(source[index])));

            accumulator = (accumulator & ~((unsigned)pixel_mask[slot])) |
                          (value << (6u - 2u * (unsigned)slot));
            ++slot;
            ++index;
        }
        *byte++ = (uint8_t)accumulator;
        slot = 0;
    }
}

void crazypod_mono_fill_row(uint8_t *row, int x, int width, unsigned value)
{
    uint8_t *byte = row + (x >> 2);
    int slot = x & 3;
    unsigned whole;

    value &= 3u;
    whole = value * 0x55u;
    /* Leading partial byte. */
    while(width > 0 && slot != 0) {
        unsigned shift = 6u - 2u * (unsigned)slot;

        *byte = (uint8_t)((*byte & ~((unsigned)pixel_mask[slot])) |
                          (value << shift));
        ++slot;
        --width;
        if(slot == 4) {
            ++byte;
            slot = 0;
        }
    }
    /* Whole bytes. */
    while(width >= 4) {
        *byte++ = (uint8_t)whole;
        width -= 4;
    }
    /* Trailing partial byte. */
    for(slot = 0; slot < width; ++slot) {
        unsigned shift = 6u - 2u * (unsigned)slot;

        *byte = (uint8_t)((*byte & ~((unsigned)pixel_mask[slot])) |
                          (value << shift));
    }
}

#endif /* HAVE_CRAZYPOD_MONO_UI */

#endif /* HAVE_CRAZYPOD_UI */
