#ifndef CRAZYPOD_GAMEBOY_TEST_CONFIG_H
#define CRAZYPOD_GAMEBOY_TEST_CONFIG_H
#define IPOD_6G 1
#define HAVE_CRAZYPOD_UI
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define ROCKBOX_LITTLE_ENDIAN 1
#define HAVE_LCD_COLOR 1
#define LCD_DEPTH 16
#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define RGB565 1
#define RGB565SWAPPED 2
#define LCD_PIXELFORMAT RGB565
/*
 * Derived from the panel in firmware/export/config.h, which this header
 * stands in for: a 160x144 Game Boy frame needs a screen at least that
 * big, so only the colour iPods build the emulator at all.
 */
#define HAVE_CRAZYPOD_GAMEBOY
#endif
