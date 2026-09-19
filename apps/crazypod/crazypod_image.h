#ifndef CRAZYPOD_IMAGE_H
#define CRAZYPOD_IMAGE_H

#include "crazypod_pixel.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lcd.h"
#include "lvgl.h"

#define CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE 4

void crazypod_image_init(void);
void crazypod_image_decode_lock(void);
void crazypod_image_decode_unlock(void);
bool crazypod_image_configure_rgb565(
    lv_image_dsc_t *descriptor, const crazypod_pixel_t *pixels,
    int width, int height);
bool crazypod_image_scale_rgb565(
    const crazypod_pixel_t *source, int source_width, int source_height,
    int source_stride, crazypod_pixel_t *destination,
    int destination_width, int destination_height);
size_t crazypod_image_glass_sample_pixels(
    int destination_width, int destination_height);
bool crazypod_image_render_glass_rgb565(
    const crazypod_pixel_t *source, int source_width, int source_height,
    int source_stride, int source_x, int source_y,
    int source_region_width, int source_region_height,
    uint32_t tint, unsigned tint_opa,
    crazypod_pixel_t *sample_pixels, crazypod_pixel_t *scratch_pixels,
    size_t sample_capacity, crazypod_pixel_t *destination,
    int destination_width, int destination_height);

#endif
