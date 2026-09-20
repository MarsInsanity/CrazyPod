#ifndef CRAZYPOD_FRAMECLOCK_TEST_CONFIG_H
#define CRAZYPOD_FRAMECLOCK_TEST_CONFIG_H

#define IPOD_6G 1
#define HAVE_CRAZYPOD_UI
#define HZ 100
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
/*
 * A 6G: a colour panel on the design's own canvas, so everything that asks
 * for one is built. These are derived in firmware/export/config.h from the
 * panel; stated here because this header stands in for that one.
 */
#define HAVE_LCD_COLOR
#define HAVE_CRAZYPOD_MINIAPPS
#define HAVE_CRAZYPOD_GAMEBOY
#define HAVE_CRAZYPOD_MEDIA_LIBRARY
#define HAVE_CRAZYPOD_WALLPAPER
#define HAVE_CRAZYPOD_ICON_THEMES
/*
 * The native carousel, which is the only thing that queues a Home present
 * band and so the only reason the frame clock holds ordinary frames back
 * while a finger is on the wheel.
 */
#define HAVE_CRAZYPOD_NATIVE_CAROUSEL

#endif
