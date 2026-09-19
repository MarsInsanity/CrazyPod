#ifndef CRAZYPOD_ICONS_H
#define CRAZYPOD_ICONS_H

#include <stdint.h>

#include "config.h"

#define CRAZYPOD_ICON_COUNT 17

struct crazypod_icon {
    const uint8_t *pixels;
    int width;
    int height;
    int stride;
};

#ifdef HAVE_CRAZYPOD_ICON_THEMES
void crazypod_icons_init(void);
void crazypod_icons_load_theme(int theme);
const struct crazypod_icon *crazypod_icon_get(int index);
#else
/*
 * No icon artwork on a monochrome panel: the themes are colour sets that
 * all quantise to the same greys, so the product's own glyphs are used
 * instead and none of this is loaded.
 */
static inline void crazypod_icons_init(void) {}

static inline void crazypod_icons_load_theme(int theme)
{
    (void)theme;
}

static inline const struct crazypod_icon *crazypod_icon_get(int index)
{
    (void)index;
    return NULL;
}
#endif

#endif
