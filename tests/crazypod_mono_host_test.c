/*
 * The monochrome panel layer, checked on the host.
 *
 * Getting the packing wrong does not fail loudly on the device -- it draws a
 * scrambled or inverted screen, which is expensive to read back from a photo
 * of an iPod. So the bit layout, the partial-byte edges and the two colour
 * maps are pinned here instead.
 */
#include "crazypod_mono.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) \
    do { \
        if(!(condition)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while(0)

#define CHECK_EQ(actual, expected) \
    do { \
        unsigned long actual_value = (unsigned long)(actual); \
        unsigned long expected_value = (unsigned long)(expected); \
        if(actual_value != expected_value) { \
            printf("FAIL %s:%d: %s == %lu, expected %lu\n", \
                   __FILE__, __LINE__, #actual, actual_value, \
                   expected_value); \
            ++failures; \
        } \
    } while(0)

/* The two bits stored for pixel `x` of a packed row. */
static unsigned stored_value(const uint8_t *row, int x)
{
    return ((unsigned)row[x >> 2] >> (6u - 2u * (unsigned)(x & 3))) & 3u;
}

static void test_hardware_value_is_inverted_brightness(void)
{
    /* The panel's darkest is 0b11, so the stored value counts down as the
     * brightness counts up. Rockbox's own 2bpp driver agrees: it expands a
     * foreground of brightness b to the byte 0x55 * (~b & 3). */
    CHECK_EQ(crazypod_mono_hardware_value(CRAZYPOD_MONO_LEVEL_INK), 3u);
    CHECK_EQ(crazypod_mono_hardware_value(CRAZYPOD_MONO_LEVEL_DARK), 2u);
    CHECK_EQ(crazypod_mono_hardware_value(CRAZYPOD_MONO_LEVEL_PALE), 1u);
    CHECK_EQ(crazypod_mono_hardware_value(CRAZYPOD_MONO_LEVEL_PAPER), 0u);
}

static void test_quantiser_is_faithful(void)
{
    /* Brightest in, brightest out: a photograph must not come back as its
     * own negative. */
    CHECK_EQ(crazypod_mono_level_from_luma(0), CRAZYPOD_MONO_LEVEL_INK);
    CHECK_EQ(crazypod_mono_level_from_luma(255), CRAZYPOD_MONO_LEVEL_PAPER);
    CHECK_EQ(crazypod_mono_level_from_luma(85), CRAZYPOD_MONO_LEVEL_DARK);
    CHECK_EQ(crazypod_mono_level_from_luma(170), CRAZYPOD_MONO_LEVEL_PALE);
    /* Monotonic across the whole range. */
    unsigned previous = 0;
    for(unsigned luma = 0; luma <= 255; ++luma) {
        unsigned level = crazypod_mono_level_from_luma(luma);

        CHECK(level >= previous);
        CHECK(level < CRAZYPOD_MONO_LEVEL_COUNT);
        previous = level;
    }
}

static void test_shade_pixels_round_trip(void)
{
    /* A cover decoded to one of the four shades has to survive the trip to
     * the panel unchanged, or the dithering it was given is wasted. */
    for(unsigned level = 0; level < CRAZYPOD_MONO_LEVEL_COUNT; ++level) {
        crazypod_pixel_t pixel = crazypod_gray_pixel(level);

        CHECK_EQ(crazypod_mono_level_from_luma(
                     crazypod_pixel_luma(pixel)), level);
    }
}

static void test_design_colours_separate(void)
{
    /* The four surfaces the product UI is built from have to land on four
     * different shades, and the page -- the darkest colour in the design --
     * has to come out as paper. This is the inverting map, and it is the
     * whole reason the monochrome build is readable. */
    CHECK_EQ(crazypod_mono_rgb(0x08080D), CRAZYPOD_MONO_PAPER);
    CHECK_EQ(crazypod_mono_rgb(0x1B1B22), CRAZYPOD_MONO_SHADE_PALE);
    CHECK_EQ(crazypod_mono_rgb(0x9A9AA4), CRAZYPOD_MONO_SHADE_DARK);
    CHECK_EQ(crazypod_mono_rgb(0xFFFFFF), CRAZYPOD_MONO_INK);

    /*
     * A white scrim the design draws at a tenth opacity over its near-black
     * page is a faint lightening of that page, and has to read as one. Taken
     * at its own colour it would be solid ink -- the same shade as the type
     * over it, which is how a legible overlay turns into a black rectangle.
     */
    CHECK(crazypod_mono_rgb_at_opacity(0xFFFFFF, 26) !=
          crazypod_mono_rgb(0xFFFFFF));
    CHECK(crazypod_mono_rgb_at_opacity(0xFFFFFF, 26) !=
          crazypod_mono_rgb(0x08080D));
    CHECK_EQ(crazypod_mono_rgb_at_opacity(0xFFFFFF, 255),
             CRAZYPOD_MONO_INK);
}

static void test_blit_row_packs_left_to_right(void)
{
    static const crazypod_pixel_t source[4] = {
        0x0000, /* ink   */
        0x52aa, /* dark  */
        0xad55, /* pale  */
        0xffff, /* paper */
    };
    uint8_t row[4];

    memset(row, 0xff, sizeof(row));
    crazypod_mono_blit_row(row, 0, 4, source);
    /* Leftmost pixel in the high bits, stored value inverted: 11 10 01 00. */
    CHECK_EQ(row[0], 0xe4u);
    CHECK_EQ(stored_value(row, 0), 3u);
    CHECK_EQ(stored_value(row, 3), 0u);
}

static void test_blit_row_keeps_its_neighbours(void)
{
    static const crazypod_pixel_t source[2] = { 0xffff, 0x0000 };
    uint8_t row[4];

    /* Start one pixel into a byte and stop one short of its end: the two
     * pixels on either side belong to a strip LVGL is not redrawing, and
     * overwriting them leaves a trail behind every partial update. */
    memset(row, 0x00, sizeof(row));
    crazypod_mono_blit_row(row, 1, 2, source);
    CHECK_EQ(stored_value(row, 0), 0u);
    CHECK_EQ(stored_value(row, 1), 0u);  /* paper -> 0 */
    CHECK_EQ(stored_value(row, 2), 3u);  /* ink   -> 3 */
    CHECK_EQ(stored_value(row, 3), 0u);

    memset(row, 0xff, sizeof(row));
    crazypod_mono_blit_row(row, 1, 2, source);
    CHECK_EQ(stored_value(row, 0), 3u);
    CHECK_EQ(stored_value(row, 1), 0u);
    CHECK_EQ(stored_value(row, 2), 3u);
    CHECK_EQ(stored_value(row, 3), 3u);
}

static void test_blit_row_spans_bytes(void)
{
    crazypod_pixel_t source[10];
    uint8_t row[5];
    int index;

    for(index = 0; index < 10; ++index)
        source[index] = (index & 1) ? 0xffff : 0x0000;
    memset(row, 0xa5, sizeof(row));
    crazypod_mono_blit_row(row, 3, 10, source);
    for(index = 0; index < 10; ++index)
        CHECK_EQ(stored_value(row, 3 + index), (index & 1) ? 0u : 3u);
    /* The pixels before and after the strip keep the 0xa5 they started
     * with, which is 10 10 01 01 read left to right. */
    CHECK_EQ(stored_value(row, 0), 2u);
    CHECK_EQ(stored_value(row, 1), 2u);
    CHECK_EQ(stored_value(row, 2), 1u);
    CHECK_EQ(stored_value(row, 13), 2u);
}

static void test_fill_row_edges(void)
{
    uint8_t row[4];
    int index;

    memset(row, 0x00, sizeof(row));
    crazypod_mono_fill_row(row, 2, 9, 3u);
    CHECK_EQ(stored_value(row, 0), 0u);
    CHECK_EQ(stored_value(row, 1), 0u);
    for(index = 2; index < 11; ++index)
        CHECK_EQ(stored_value(row, index), 3u);
    CHECK_EQ(stored_value(row, 11), 0u);

    /* A whole-byte run writes the packed byte directly. */
    memset(row, 0x00, sizeof(row));
    crazypod_mono_fill_row(row, 0, 8, 2u);
    CHECK_EQ(row[0], 0xaau);
    CHECK_EQ(row[1], 0xaau);
    CHECK_EQ(row[2], 0x00u);

    /* A zero-width fill changes nothing. */
    memset(row, 0x5a, sizeof(row));
    crazypod_mono_fill_row(row, 1, 0, 3u);
    CHECK_EQ(row[0], 0x5au);
}

int main(void)
{
    test_hardware_value_is_inverted_brightness();
    test_quantiser_is_faithful();
    test_shade_pixels_round_trip();
    test_design_colours_separate();
    test_blit_row_packs_left_to_right();
    test_blit_row_keeps_its_neighbours();
    test_blit_row_spans_bytes();
    test_fill_row_edges();

    if(failures != 0) {
        printf("crazypod_mono_host_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("crazypod_mono_host_test: all checks passed\n");
    return 0;
}
