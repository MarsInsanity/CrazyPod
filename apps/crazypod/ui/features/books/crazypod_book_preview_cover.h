#ifndef CRAZYPOD_BOOK_PREVIEW_COVER_H
#define CRAZYPOD_BOOK_PREVIEW_COVER_H

#include "lvgl.h"

#include "../../../crazypod_books.h"

/* Covers decode synchronously, so they wait for the wheel to stop. */
void crazypod_book_preview_cover_mark(int key, long now);
bool crazypod_book_preview_cover_settled(long now);
bool crazypod_book_preview_cover_waiting(long now, long *due);

/*
 * One decode per draw.
 *
 * The Books menu draws a stack -- three covers for Recents, four for the
 * shelf -- and every one of them opened an epub and decoded a JPEG in the
 * same render. Holding them until the wheel stopped only moved that from
 * the scroll to the moment you stop on it. Begin a pass, and the first
 * cover that is not already decoded is the only one that costs anything;
 * the rest draw as sleeves and say so through _deferred(), so the caller
 * can ask to be drawn again and take the next one.
 */
void crazypod_book_preview_cover_begin_pass(void);
bool crazypod_book_preview_cover_deferred(void);

lv_obj_t *crazypod_book_preview_cover_create(
    lv_obj_t *parent, const struct crazypod_book *book,
    int x, int y, int width, int height);

#endif
