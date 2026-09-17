#ifndef CRAZYPOD_BOOK_COVER_H
#define CRAZYPOD_BOOK_COVER_H

#include "lvgl.h"

const lv_image_dsc_t *crazypod_book_cover_get(
    int book_index, int max_width, int max_height);
/*
 * The same cover, but only if it is already decoded and in memory. It
 * touches no file and probes nothing, so a preview can ask about several
 * books and find out which of them would cost something.
 */
const lv_image_dsc_t *crazypod_book_cover_ready(
    int book_index, int max_width, int max_height);
void crazypod_book_cover_reset(void);

#endif
