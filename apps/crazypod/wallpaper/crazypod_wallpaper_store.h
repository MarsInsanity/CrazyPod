#ifndef CRAZYPOD_WALLPAPER_STORE_H
#define CRAZYPOD_WALLPAPER_STORE_H

#include "crazypod_pixel.h"
#include "lcd.h"
#include "lvgl.h"

#include "../crazypod_wallpaper.h"

void crazypod_wallpaper_store_init(void);
bool crazypod_wallpaper_store_load(
    enum crazypod_wallpaper_target target,
    const char *source_path, crazypod_pixel_t *pixels,
    lv_image_dsc_t *descriptor);
bool crazypod_wallpaper_store_save(
    enum crazypod_wallpaper_target target,
    const char *source_path, const crazypod_pixel_t *pixels,
    enum crazypod_wallpaper_apply_result *error);
void crazypod_wallpaper_store_remove(
    enum crazypod_wallpaper_target target);

#endif
