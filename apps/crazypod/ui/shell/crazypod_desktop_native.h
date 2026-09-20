#ifndef CRAZYPOD_DESKTOP_NATIVE_H
#define CRAZYPOD_DESKTOP_NATIVE_H

#include <stdbool.h>

#include "config.h"
#include "lcd.h"
#include "lvgl.h"

/*
 * The band of the home screen the icon carousel owns, and how big an icon
 * gets to be in it. The carousel is drawn straight into the framebuffer
 * rather than through LVGL, so this is the one place its extent is stated;
 * everything else -- the backdrop it caches, the modal underlay it hands
 * to a popup, the pre-scaled icons it keeps -- is sized from it.
 */
#ifdef HAVE_CRAZYPOD_COMPACT_UI
#define CRAZYPOD_DESKTOP_NATIVE_TOP 14
#define CRAZYPOD_DESKTOP_NATIVE_BOTTOM 62
#define CRAZYPOD_DESKTOP_NATIVE_MAX_ICON 34
#else
#define CRAZYPOD_DESKTOP_NATIVE_TOP 40
#define CRAZYPOD_DESKTOP_NATIVE_BOTTOM 143
#define CRAZYPOD_DESKTOP_NATIVE_MAX_ICON 120
#endif
#define CRAZYPOD_DESKTOP_NATIVE_CENTER_X (LCD_WIDTH / 2)
#define CRAZYPOD_DESKTOP_NATIVE_CENTER_Y \
    ((CRAZYPOD_DESKTOP_NATIVE_TOP + CRAZYPOD_DESKTOP_NATIVE_BOTTOM) / 2)
#define CRAZYPOD_DESKTOP_NATIVE_MAX_VISIBLE 5

#ifdef HAVE_CRAZYPOD_NATIVE_CAROUSEL
void crazypod_desktop_native_reset(void);
void crazypod_desktop_native_invalidate(bool discard_backdrop);
void crazypod_desktop_native_invalidate_icons(void);
void crazypod_desktop_native_prepare_modal(void);
void crazypod_desktop_native_restore_after_modal(void);
lv_obj_t *crazypod_desktop_native_create_modal_underlay(
    lv_obj_t *parent);
void crazypod_desktop_native_preserve_modal_underlay(void);
void crazypod_desktop_native_capture_flush(const lv_area_t *area);
bool crazypod_desktop_native_render(
    const int *app_indices, const int *centers_x, int icon_count,
    int icon_size, bool blocked);
bool crazypod_desktop_native_render_snapshot(
    const int *app_indices, const int *centers_x, int icon_count,
    int icon_size);
#else
/*
 * The carousel is drawn straight into the framebuffer in the panel's own
 * format, because five 120-pixel icons a frame were too much to ask of
 * LVGL at 320x240. The Mini's framebuffer is two bits a pixel, and its
 * carousel is three icons of fifty, which LVGL draws without complaint --
 * so the home screen builds it out of ordinary widgets there and none of
 * this is compiled.
 */
static inline void crazypod_desktop_native_reset(void) {}

static inline void crazypod_desktop_native_invalidate(
    bool discard_backdrop)
{
    (void)discard_backdrop;
}

static inline void crazypod_desktop_native_invalidate_icons(void) {}
static inline void crazypod_desktop_native_prepare_modal(void) {}
static inline void crazypod_desktop_native_restore_after_modal(void) {}

static inline lv_obj_t *crazypod_desktop_native_create_modal_underlay(
    lv_obj_t *parent)
{
    (void)parent;
    return NULL;
}

static inline void crazypod_desktop_native_preserve_modal_underlay(void) {}

static inline void crazypod_desktop_native_capture_flush(
    const lv_area_t *area)
{
    (void)area;
}
#endif

#endif
