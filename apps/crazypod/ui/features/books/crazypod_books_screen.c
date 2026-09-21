#include "config.h"

#include "../../../crazypod_l10n.h"

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_books.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_books_screen.h"

#include "../../presentation/crazypod_ui_metrics.h"
#include "../../../crazypod_mono.h"
#include "../../../crazypod_runtime_font.h"

#define CRAZYPOD_BOOKS_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_BOOKS_WHITE 0xFFFFFF
#define CRAZYPOD_BOOKS_PANEL 0x1B1B22

#ifdef HAVE_CRAZYPOD_COMPACT_UI
/*
 * The reader page, its toolbar and the two card screens were all written
 * against 320x240: a 300px column of type starting at 36, a 34px toolbar
 * at 206 with its controls at 47, 151 and 255, and 292x126 cards at 74.
 * None of it was on this panel.
 */
#define BOOK_TEXT_X 4
#define BOOK_TEXT_Y (CRAZYPOD_METRIC_STATUS_HEIGHT + 2)
#define BOOK_TEXT_WIDTH (LCD_WIDTH - 8)
#define BOOK_TEXT_HEIGHT_BARE (LCD_HEIGHT - BOOK_TEXT_Y - 2)
#define BOOK_TEXT_HEIGHT_TOOLBAR (BOOK_TEXT_HEIGHT_BARE - 16)
#define BOOK_TOOLBAR_HEIGHT 16
#define BOOK_TOOLBAR_Y (LCD_HEIGHT - BOOK_TOOLBAR_HEIGHT)
#define BOOK_ARROW_FONT (&lv_font_montserrat_12)
#define BOOK_LIST_FONT (&lv_font_montserrat_10)
#define BOOK_PREV_X 46
#define BOOK_PREV_Y 2
#define BOOK_LIST_X 70
#define BOOK_LIST_Y 3
#define BOOK_NEXT_X 98
#define BOOK_NEXT_Y 2
#define BOOK_PERCENT_X 3
#define BOOK_PERCENT_Y 4
#define BOOK_PERCENT_WIDTH 30
#define BOOK_PERCENT_ALIGN LV_TEXT_ALIGN_LEFT
#define BOOK_CARD_TITLE_FONT (&lv_font_montserrat_12)
#define BOOK_CARD_TITLE_X 4
#define BOOK_CARD_TITLE_Y (CRAZYPOD_METRIC_STATUS_HEIGHT + 1)
#define BOOK_CARD_X 3
#define BOOK_CARD_Y (CRAZYPOD_METRIC_STATUS_HEIGHT + 15)
#define BOOK_CARD_WIDTH (LCD_WIDTH - 6)
#define BOOK_CARD_HEIGHT (LCD_HEIGHT - BOOK_CARD_Y - 3)
#define BOOK_CARD_RADIUS 4
#define BOOK_CARD_TEXT_X 4
#define BOOK_CARD_TEXT_Y 3
#define BOOK_CARD_TEXT_WIDTH (BOOK_CARD_WIDTH - 10)
#define BOOK_CARD_TEXT_HEIGHT (BOOK_CARD_HEIGHT - 8)
#define BOOK_CARD_FONT (crazypod_runtime_font_at_size(12))
#else
#define BOOK_TEXT_X 10
#define BOOK_TEXT_Y 36
#define BOOK_TEXT_WIDTH 300
#define BOOK_TEXT_HEIGHT_BARE 196
#define BOOK_TEXT_HEIGHT_TOOLBAR 164
#define BOOK_TOOLBAR_HEIGHT 34
#define BOOK_TOOLBAR_Y 206
#define BOOK_ARROW_FONT (&lv_font_montserrat_16)
#define BOOK_LIST_FONT (&lv_font_montserrat_12)
#define BOOK_PREV_X 47
#define BOOK_PREV_Y 6
#define BOOK_LIST_X 151
#define BOOK_LIST_Y 7
#define BOOK_NEXT_X 255
#define BOOK_NEXT_Y 6
#define BOOK_PERCENT_X 139
#define BOOK_PERCENT_Y 23
#define BOOK_PERCENT_WIDTH 42
#define BOOK_PERCENT_ALIGN LV_TEXT_ALIGN_CENTER
#define BOOK_CARD_TITLE_FONT (&lv_font_montserrat_16)
#define BOOK_CARD_TITLE_X 14
#define BOOK_CARD_TITLE_Y 43
#define BOOK_CARD_X 14
#define BOOK_CARD_Y 74
#define BOOK_CARD_WIDTH 292
#define BOOK_CARD_HEIGHT 132
#define BOOK_CARD_RADIUS 10
#define BOOK_CARD_TEXT_X 13
#define BOOK_CARD_TEXT_Y 11
#define BOOK_CARD_TEXT_WIDTH 266
#define BOOK_CARD_TEXT_HEIGHT 112
#define BOOK_CARD_FONT CRAZYPOD_BOOKS_FONT
#endif
/* The type is drawn scaled, so its box has to grow by the same ratio. */
#define BOOK_SMALL_SCALE 224
#define BOOK_SCALED(value) ((value) * 256 / BOOK_SMALL_SCALE)

void crazypod_books_screen_render_reader(
    lv_obj_t *content, int book_index, uint32_t page_offset,
    const char *page_text, uint32_t page_color, uint32_t ink_color,
    bool toolbar_visible)
{
    const struct crazypod_book *book =
        crazypod_book_get(book_index);
    int theme = crazypod_books_theme();
    int font_size = crazypod_books_font_size();
    const lv_font_t *reader_font = font_size == 2
        ? &lv_font_source_han_sans_sc_16_cjk
        : CRAZYPOD_BOOKS_FONT;
    lv_obj_t *page;
    lv_obj_t *toolbar;
    lv_obj_t *label;
    char progress[24];
    uint32_t total = book != NULL && book->content_size > 0
        ? book->content_size : book != NULL ? book->size : 0;
    unsigned percent = total > 0
        ? page_offset * 100u / total : 0;

    page = crazypod_ui_widget_box(
        content, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
        page_color, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        page,
        page_text[0] != '\0'
            ? page_text : CP_TR("This book could not be decoded."),
        reader_font, ink_color,
        LV_OPA_COVER);
    lv_obj_set_pos(label, BOOK_TEXT_X, BOOK_TEXT_Y);
    lv_obj_set_width(label, BOOK_TEXT_WIDTH);
    lv_obj_set_height(
        label,
        toolbar_visible
            ? BOOK_TEXT_HEIGHT_TOOLBAR : BOOK_TEXT_HEIGHT_BARE);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    {
        static const int scales[] = { BOOK_SMALL_SCALE, 256, 256 };

        lv_obj_set_style_transform_scale_x(
            label, scales[font_size], 0);
        lv_obj_set_style_transform_scale_y(
            label, scales[font_size], 0);
        lv_obj_set_style_transform_pivot_x(label, 0, 0);
        lv_obj_set_style_transform_pivot_y(label, 0, 0);
        if(font_size == 0) {
            lv_obj_set_width(label, BOOK_SCALED(BOOK_TEXT_WIDTH));
            lv_obj_set_height(
                label,
                BOOK_SCALED(
                    toolbar_visible
                        ? BOOK_TEXT_HEIGHT_TOOLBAR
                        : BOOK_TEXT_HEIGHT_BARE));
        }
    }

    if(!toolbar_visible)
        return;
    toolbar = crazypod_ui_widget_box(
        page, 0, BOOK_TOOLBAR_Y, LCD_WIDTH, BOOK_TOOLBAR_HEIGHT, 0,
        theme == 3 ? 0xFFFFFF : 0x000000,
        theme == 3 ? 24 : 15);
    label = crazypod_ui_widget_label(toolbar, LV_SYMBOL_LEFT,
                       BOOK_ARROW_FONT,
                       ink_color, 155);
    lv_obj_set_pos(label, BOOK_PREV_X, BOOK_PREV_Y);
    label = crazypod_ui_widget_label(
        toolbar,
        LV_SYMBOL_LIST,
        BOOK_LIST_FONT,
        ink_color, 155);
    lv_obj_set_pos(label, BOOK_LIST_X, BOOK_LIST_Y);
    label = crazypod_ui_widget_label(toolbar, LV_SYMBOL_RIGHT,
                       BOOK_ARROW_FONT,
                       ink_color, 155);
    lv_obj_set_pos(label, BOOK_NEXT_X, BOOK_NEXT_Y);
    snprintf(progress, sizeof(progress), "%u%%", percent);
    label = crazypod_ui_widget_label(toolbar, progress, &lv_font_montserrat_8,
                       ink_color, 115);
    lv_obj_set_width(label, BOOK_PERCENT_WIDTH);
    lv_obj_set_style_text_align(label, BOOK_PERCENT_ALIGN, 0);
    lv_obj_set_pos(label, BOOK_PERCENT_X, BOOK_PERCENT_Y);
}

/*
 * The card the Stats and Info screens are printed on. The design's panel
 * is near-black at four fifths opacity, which the monochrome map turns
 * into the page it is drawn on, so the card is named in shades there.
 */
static lv_obj_t *book_card(lv_obj_t *content, const char *title)
{
    lv_obj_t *label;

#ifdef HAVE_CRAZYPOD_MONO_UI
    label = crazypod_ui_widget_label_shade(
        content, title, BOOK_CARD_TITLE_FONT,
        CRAZYPOD_MONO_INK, LV_OPA_COVER);
#else
    label = crazypod_ui_widget_label(
        content, title, BOOK_CARD_TITLE_FONT,
        CRAZYPOD_BOOKS_WHITE, LV_OPA_COVER);
#endif
    lv_obj_set_pos(label, BOOK_CARD_TITLE_X, BOOK_CARD_TITLE_Y);
#ifdef HAVE_CRAZYPOD_MONO_UI
    return crazypod_ui_widget_box_shade(
        content, BOOK_CARD_X, BOOK_CARD_Y,
        BOOK_CARD_WIDTH, BOOK_CARD_HEIGHT, BOOK_CARD_RADIUS,
        CRAZYPOD_MONO_SHADE_PALE, LV_OPA_COVER);
#else
    return crazypod_ui_widget_box(
        content, BOOK_CARD_X, BOOK_CARD_Y,
        BOOK_CARD_WIDTH, BOOK_CARD_HEIGHT, BOOK_CARD_RADIUS,
        CRAZYPOD_BOOKS_PANEL, 220);
#endif
}

static lv_obj_t *book_card_text(lv_obj_t *panel, const char *text)
{
    lv_obj_t *label;

#ifdef HAVE_CRAZYPOD_MONO_UI
    label = crazypod_ui_widget_label_shade(
        panel, text, BOOK_CARD_FONT,
        CRAZYPOD_MONO_INK, LV_OPA_COVER);
#else
    label = crazypod_ui_widget_label(
        panel, text, BOOK_CARD_FONT,
        CRAZYPOD_BOOKS_WHITE, 230);
#endif
    lv_obj_set_pos(label, BOOK_CARD_TEXT_X, BOOK_CARD_TEXT_Y);
    lv_obj_set_width(label, BOOK_CARD_TEXT_WIDTH);
    return label;
}

void crazypod_books_screen_render_stats(lv_obj_t *content)
{
    lv_obj_t *label;
    lv_obj_t *panel;
    char text[160];

    panel = book_card(content, CP_TR("READING STATS"));
    snprintf(text, sizeof(text),
             CP_FMT("%d books\n%d recently opened\n%d favorites\n\n"
                    "Progress is stored on this iPod."),
             crazypod_books_count(),
             crazypod_books_recent_count(),
             crazypod_books_favorite_count());
    label = book_card_text(panel, text);
    lv_obj_set_height(label, BOOK_CARD_TEXT_HEIGHT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}

void crazypod_books_screen_render_info(lv_obj_t *content,
                                       int book_index)
{
    const struct crazypod_book *book;
    lv_obj_t *label;
    lv_obj_t *panel;
    char text[256];
    const char *format;

    crazypod_book_probe(book_index);
    book = crazypod_book_get(book_index);
    format = book == NULL ? "" :
        book->format == CRAZYPOD_BOOK_TXT ? CP_TR("TXT") :
        book->format == CRAZYPOD_BOOK_MARKDOWN ? CP_TR("Markdown") : CP_TR("EPUB");

    panel = book_card(content, CP_TR("BOOK INFO"));
    snprintf(text, sizeof(text),
             CP_FMT("%.60s\n%s%.60s%s\n%.8s · %lu KB\n\n%.80s"),
             book != NULL ? book->title : CP_FMT("Missing Book"),
             book != NULL && book->author[0] != '\0' ? CP_FMT("by ") : "",
             book != NULL ? book->author : "",
             book != NULL && book->author[0] != '\0' ? "" : CP_FMT("Unknown author"),
             format,
             (unsigned long)(book != NULL ? book->size / 1024u : 0),
             book != NULL ? book->path : "");
    label = book_card_text(panel, text);
    lv_obj_set_height(label, BOOK_CARD_TEXT_HEIGHT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}
