#ifndef CRAZYPOD_BOOK_PREVIEW_COVER_H
#define CRAZYPOD_BOOK_PREVIEW_COVER_H

#include "lvgl.h"

#include "../../../crazypod_books.h"

/* Covers decode synchronously, so they wait for the wheel to stop. */
void crazypod_book_preview_cover_mark(int key, long now);
bool crazypod_book_preview_cover_settled(long now);
bool crazypod_book_preview_cover_waiting(long now, long *due);

lv_obj_t *crazypod_book_preview_cover_create(
    lv_obj_t *parent, const struct crazypod_book *book,
    int x, int y, int width, int height);

#endif
