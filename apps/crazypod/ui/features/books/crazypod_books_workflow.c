#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include <stdio.h>
#include <string.h>

#include "../../../crazypod_books.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_book_session.h"
#include "crazypod_books_workflow.h"
#include "../../../crazypod_color.h"

#ifdef HAVE_CRAZYPOD_COMPACT_UI
/*
 * The book-loading screen was a 320 wide layout: a 280px title, a 240px
 * progress track starting at 40, and a caption at 181. None of it was on
 * a 138x110 panel.
 */
#define LOADING_TITLE_FONT (&lv_font_montserrat_12)
#define LOADING_TITLE_Y 20
#define LOADING_SUBTITLE_Y 37
#define LOADING_INSET 4
#define LOADING_TRACK_X 8
#define LOADING_TRACK_Y 56
#define LOADING_TRACK_WIDTH (LCD_WIDTH - 16)
#define LOADING_TRACK_HEIGHT 5
#define LOADING_STAGE_X 8
#define LOADING_STAGE_Y 66
#define LOADING_STAGE_WIDTH 84
#define LOADING_PERCENT_X (LCD_WIDTH - 42)
#define LOADING_PERCENT_WIDTH 34
#define LOADING_DETAIL_Y 84
#else
#define LOADING_TITLE_FONT (&lv_font_montserrat_16)
#define LOADING_TITLE_Y 57
#define LOADING_SUBTITLE_Y 88
#define LOADING_INSET 20
#define LOADING_TRACK_X 40
#define LOADING_TRACK_Y 126
#define LOADING_TRACK_WIDTH 240
#define LOADING_TRACK_HEIGHT 7
#define LOADING_STAGE_X 40
#define LOADING_STAGE_Y 145
#define LOADING_STAGE_WIDTH 220
#define LOADING_PERCENT_X 238
#define LOADING_PERCENT_WIDTH 42
#define LOADING_DETAIL_Y 181
#endif

static struct crazypod_books_workflow_host workflow_host;
static bool metadata_ready;
static lv_obj_t *progress_fill;
static lv_obj_t *progress_label;
static lv_obj_t *percent_label;

void crazypod_books_workflow_configure(
    const struct crazypod_books_workflow_host *host)
{
    memset(&workflow_host, 0, sizeof(workflow_host));
    if(host != NULL)
        workflow_host = *host;
}

void crazypod_books_workflow_reset_view(void)
{
    progress_fill = NULL;
    progress_label = NULL;
    percent_label = NULL;
}

void crazypod_books_workflow_invalidate_metadata(void)
{
    metadata_ready = false;
    crazypod_book_session_reset();
}

static void render_loading(
    const struct crazypod_book *book,
    const char *title, const char *detail)
{
    int theme = crazypod_books_theme();
    uint32_t page_color = workflow_host.page_colors[theme];
    uint32_t ink_color = workflow_host.ink_colors[theme];
    lv_obj_t *label;
    lv_obj_t *track;

    lv_obj_clean(workflow_host.parent);
    lv_obj_set_style_bg_color(
        workflow_host.parent, crazypod_ui_color(page_color), 0);
    lv_obj_set_style_bg_opa(
        workflow_host.parent, LV_OPA_COVER, 0);
    crazypod_ui_widget_box(
        workflow_host.parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, page_color, LV_OPA_COVER);
    workflow_host.set_status_palette(ink_color, page_color);

    label = crazypod_ui_widget_label(
        workflow_host.parent,
        title != NULL ? title : CP_TR("Preparing Book"),
        LOADING_TITLE_FONT, ink_color, LV_OPA_COVER);
    lv_obj_set_width(label, LCD_WIDTH - 2 * LOADING_INSET);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, LOADING_INSET, LOADING_TITLE_Y);

    label = crazypod_ui_widget_label(
        workflow_host.parent,
        book != NULL && book->title[0] != '\0'
            ? book->title : CP_TR("Reading local book data"),
        workflow_host.metadata_font, ink_color, 180);
    lv_obj_set_width(label, LCD_WIDTH - 2 * LOADING_INSET);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, LOADING_INSET, LOADING_SUBTITLE_Y);

    track = crazypod_ui_widget_box(
        workflow_host.parent,
        LOADING_TRACK_X, LOADING_TRACK_Y,
        LOADING_TRACK_WIDTH, LOADING_TRACK_HEIGHT,
        LV_RADIUS_CIRCLE, ink_color, 32);
    progress_fill = crazypod_ui_widget_box(
        track, 0, 0, 2, LOADING_TRACK_HEIGHT,
        LV_RADIUS_CIRCLE, ink_color, 220);
    progress_label = crazypod_ui_widget_label(
        workflow_host.parent, CP_TR("Starting"),
        &lv_font_montserrat_10, ink_color, 190);
    lv_obj_set_width(progress_label, LOADING_STAGE_WIDTH);
    lv_label_set_long_mode(
        progress_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(
        progress_label, LOADING_STAGE_X, LOADING_STAGE_Y);
    percent_label = crazypod_ui_widget_label(
        workflow_host.parent, "0%",
        &lv_font_montserrat_10, ink_color, 190);
    lv_obj_set_width(percent_label, LOADING_PERCENT_WIDTH);
    lv_obj_set_style_text_align(
        percent_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(
        percent_label, LOADING_PERCENT_X, LOADING_STAGE_Y);
    label = crazypod_ui_widget_label(
        workflow_host.parent, detail != NULL ? detail : "",
        &lv_font_montserrat_8, ink_color, 105);
    lv_obj_set_width(label, LCD_WIDTH - 2 * LOADING_INSET);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, LOADING_INSET, LOADING_DETAIL_Y);
    workflow_host.status_foreground();
    lv_refr_now(NULL);
    workflow_host.present();
}

static void update_progress(
    int percent, const char *stage, void *context)
{
    char value[16];
    int width;

    (void)context;
    if(progress_fill == NULL ||
       progress_label == NULL || percent_label == NULL)
        return;
    if(percent < 0)
        percent = 0;
    if(percent > 100)
        percent = 100;
    width = percent * LOADING_TRACK_WIDTH / 100;
    if(width < 2)
        width = 2;
    lv_obj_set_width(progress_fill, width);
    CP_LV_LABEL_SET_TEXT(
        progress_label, stage != NULL ? stage : CP_TR("Preparing"));
    snprintf(value, sizeof(value), CP_FMT("%d%%"), percent);
    CP_LV_LABEL_SET_TEXT(percent_label, value);
    lv_refr_now(NULL);
    workflow_host.present();
}

void crazypod_books_workflow_ensure_metadata(void)
{
    bool scan_needed = crazypod_books_scan_needed();

    if(metadata_ready && !scan_needed)
        return;
    if(!scan_needed) {
        metadata_ready = true;
        return;
    }
    render_loading(
        NULL, CP_TR("Loading Library"),
        CP_TR("Scanning Books folder"));
    update_progress(12, CP_TR("Scanning Books folder"), NULL);
    crazypod_books_scan();
    metadata_ready = true;
    update_progress(100, CP_TR("Library ready"), NULL);
}

void crazypod_books_workflow_apply_font_size(int value)
{
    const struct crazypod_book *book =
        crazypod_book_get(crazypod_book_session_index());
    bool needs_reflow =
        book != NULL && crazypod_book_session_has_text();

    if(value == crazypod_books_font_size()) {
        workflow_host.render_route(false);
        return;
    }
    if(needs_reflow) {
        render_loading(
            book, CP_TR("Reflowing Text"),
            CP_TR("Keeping your current reading position"));
        update_progress(20, CP_TR("Applying text size"), NULL);
    }
    crazypod_books_set_font_size(value);
    if(needs_reflow) {
        bool loaded;
        int index = crazypod_book_session_index();
        uint32_t offset = crazypod_book_session_offset();

        update_progress(62, CP_TR("Rebuilding current page"), NULL);
        crazypod_book_session_begin(index);
        loaded = crazypod_book_session_load(index, offset);
        update_progress(
            100, loaded ? CP_TR("Text ready")
                        : CP_TR("Could not reload page"), NULL);
    }
    workflow_host.render_route(false);
}

void crazypod_books_workflow_begin_reader(
    int index, uint32_t offset)
{
    const struct crazypod_book *book;
    bool show_progress;
    bool ready;
    bool loaded = false;

    book = crazypod_book_get(index);
    if(book == NULL)
        return;
    show_progress =
        book->format == CRAZYPOD_BOOK_EPUB &&
        book->content_size == 0;
    if(show_progress)
        render_loading(
            book, CP_TR("Preparing Book"),
            CP_TR("First open creates a local reading cache"));
    ready = show_progress
        ? crazypod_book_prepare_with_progress(
              index, update_progress, NULL)
        : crazypod_book_prepare(index);
    book = crazypod_book_get(index);
    if(book == NULL)
        return;
    if(book->content_size > 0 && offset >= book->content_size)
        offset = 0;
    crazypod_book_session_begin(index);
    if(show_progress)
        update_progress(
            94, ready ? CP_TR("Loading first page")
                      : CP_TR("Preparing error details"), NULL);
    if(ready)
        loaded = crazypod_book_session_load(index, offset);
    if(!loaded) {
        crazypod_book_session_set_error(
            book->format == CRAZYPOD_BOOK_EPUB
                ? CP_TR("This EPUB is damaged, encrypted, or unsupported.")
                : CP_TR("Unable to open this book."));
    }
    else {
        if(show_progress)
            update_progress(98, CP_TR("Saving reading position"), NULL);
        crazypod_book_set_progress(index, offset);
    }
    if(show_progress)
        update_progress(
            100, loaded ? CP_TR("Opening reader")
                        : CP_TR("Showing book error"), NULL);
    workflow_host.push_reader(index);
}

#endif
