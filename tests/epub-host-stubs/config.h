#ifndef EPUB_HOST_TEST_CONFIG_H
#define EPUB_HOST_TEST_CONFIG_H

#define IPOD_6G 1
#define HAVE_CRAZYPOD_UI
#define MAX_PATH 1024
#define HAVE_LCD_COLOR 1

/*
 * firmware/common/zip.c calls strlcpy, which the host's string.h only
 * declares under _DEFAULT_SOURCE -- and that in turn redefines the endian
 * macros the stubs provide. Declare it here instead; glibc has carried the
 * symbol since 2.38 and the BSDs always have.
 */
#include <stddef.h>
size_t strlcpy(char *dst, const char *src, size_t size);

#endif
