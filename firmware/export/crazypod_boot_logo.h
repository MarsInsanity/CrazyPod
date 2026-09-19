#ifndef CRAZYPOD_BOOT_LOGO_H
#define CRAZYPOD_BOOT_LOGO_H

#include "crazypod_pixel.h"

#define CRAZYPOD_BOOT_LOGO_WIDTH 33
#define CRAZYPOD_BOOT_LOGO_HEIGHT 40

const crazypod_pixel_t *crazypod_boot_logo_pixels(void);
void crazypod_boot_logo_draw(crazypod_pixel_t *framebuffer,
                             int framebuffer_width,
                             int framebuffer_height,
                             int framebuffer_stride);

#endif
