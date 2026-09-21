#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include <stdio.h>

#include "lvgl.h"

#include "crazypod_ui_menu_layout.h"
#include "crazypod_ui_metrics.h"
#include "crazypod_ui_widgets.h"
#include "crazypod_menu_list.h"
#include "crazypod_search_screen.h"
#include "../../crazypod_color.h"
#include "../../crazypod_mono.h"

#define COLOR_WHITE 0xFFFFFF
#ifdef HAVE_CRAZYPOD_MONO_UI
/*
 * The same shades the menu list names for the same reason: the design
 * paints the selection bar in the accent and the title on it in white, and
 * both of those land on ink here, so a selected row came out as dark type
 * on a dark bar. See crazypod_menu_screen.c.
 */
#define ROW_SELECTED_FILL CRAZYPOD_MONO_INK
#define ROW_SELECTED_TEXT CRAZYPOD_MONO_PAPER
#define ROW_TEXT CRAZYPOD_MONO_INK
#define HEADER_TEXT CRAZYPOD_MONO_SHADE_DARK
#define MARKER_TEXT CRAZYPOD_MONO_SHADE_DARK
#define QUERY_TEXT CRAZYPOD_MONO_INK
#define QUERY_HINT CRAZYPOD_MONO_SHADE_DARK
#endif
#define CRAZYPOD_VISIBLE_ROWS CRAZYPOD_METRIC_SEARCH_ROWS
#define CRAZYPOD_SEARCH_PREVIEW_ROWS 2
#define CRAZYPOD_MENU_HEADER_X CRAZYPOD_METRIC_MENU_HEADER_X
#define CRAZYPOD_MENU_HEADER_Y CRAZYPOD_METRIC_MENU_HEADER_Y
#define CRAZYPOD_MENU_HEADER_WIDTH CRAZYPOD_METRIC_MENU_HEADER_WIDTH
#define CRAZYPOD_MENU_HEADER_HEIGHT CRAZYPOD_METRIC_MENU_HEADER_HEIGHT
#define CRAZYPOD_MENU_ROW_X CRAZYPOD_METRIC_MENU_ROW_X
#define CRAZYPOD_MENU_ROW_Y CRAZYPOD_METRIC_SEARCH_ROWS_Y
#define CRAZYPOD_MENU_ROW_WIDTH CRAZYPOD_METRIC_MENU_ROW_WIDTH
#define CRAZYPOD_MENU_ROW_HEIGHT CRAZYPOD_METRIC_MENU_ROW_HEIGHT
#define CRAZYPOD_MENU_ROW_STEP CRAZYPOD_METRIC_MENU_ROW_STEP

static lv_obj_t *make_box(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_box(
        parent, x, y, width, height, radius, color, opacity);
}

static lv_obj_t *make_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_label(
        parent, text, font, color, opacity);
}

void crazypod_search_screen_render(
    const struct route_state *state,
    const struct crazypod_search_screen_context *context)
{
    int count = context->item_count;
    int start;
    int row;
    int result_count = context->query[0] != '\0'
        ? context->result_count(context->query) : 0;
    lv_obj_t *label;
    lv_obj_t *query_box;
    char text[96];

    crazypod_menu_list_reset(state->route);

#ifdef HAVE_CRAZYPOD_MONO_UI
    label = crazypod_ui_widget_label_shade(
        context->parent, CP_TR("SEARCH"),
        context->metadata_font, HEADER_TEXT, LV_OPA_COVER);
#else
    label = make_label(context->parent, CP_TR("SEARCH"),
                       context->metadata_font,
                       COLOR_WHITE, 85);
#endif
    lv_obj_set_pos(label, CRAZYPOD_MENU_HEADER_X,
                   CRAZYPOD_MENU_HEADER_Y);
    lv_obj_set_width(label, CRAZYPOD_MENU_HEADER_WIDTH);
    lv_obj_set_height(label, CRAZYPOD_MENU_HEADER_HEIGHT);

    query_box = context->make_panel(
        context->parent, CRAZYPOD_GLASS_SLOT_SEARCH_QUERY,
        CRAZYPOD_METRIC_SEARCH_QUERY_X,
        CRAZYPOD_METRIC_SEARCH_QUERY_Y,
        CRAZYPOD_METRIC_SEARCH_QUERY_WIDTH,
        CRAZYPOD_METRIC_SEARCH_QUERY_HEIGHT,
        CRAZYPOD_METRIC_SEARCH_QUERY_RADIUS);
    if(context->query[0] != '\0') {
        lv_obj_t *active = make_box(
            query_box, 0, 0,
            CRAZYPOD_METRIC_SEARCH_QUERY_WIDTH,
            CRAZYPOD_METRIC_SEARCH_QUERY_HEIGHT,
            CRAZYPOD_METRIC_SEARCH_QUERY_RADIUS,
            context->primary_color, 82);

        if(context->gradient_highlight) {
            lv_obj_set_style_bg_grad_color(
                active, crazypod_ui_color(context->secondary_color), 0);
            lv_obj_set_style_bg_grad_dir(active, LV_GRAD_DIR_HOR, 0);
        }
        lv_obj_remove_flag(active, LV_OBJ_FLAG_CLICKABLE);
    }
    label = make_label(query_box, LV_SYMBOL_KEYBOARD,
                       CRAZYPOD_METRIC_SEARCH_QUERY_ICON_FONT,
                       COLOR_WHITE, context->query[0] != '\0' ? 235 : 90);
    lv_obj_set_pos(label,
                   CRAZYPOD_METRIC_SEARCH_QUERY_ICON_X,
                   CRAZYPOD_METRIC_SEARCH_QUERY_ICON_Y);
#ifdef HAVE_CRAZYPOD_MONO_UI
    label = crazypod_ui_widget_label_shade(
        query_box,
        context->query[0] != '\0'
            ? context->query : CP_TR("Start typing"),
        context->metadata_font,
        context->query[0] != '\0' ? QUERY_TEXT : QUERY_HINT,
        LV_OPA_COVER);
#else
    label = make_label(query_box,
                       context->query[0] != '\0'
                           ? context->query : CP_TR("Start typing"),
                       context->metadata_font,
                       COLOR_WHITE,
                       context->query[0] != '\0' ? 255 : 120);
#endif
    lv_obj_set_pos(label,
                   CRAZYPOD_METRIC_SEARCH_QUERY_TEXT_X,
                   CRAZYPOD_METRIC_SEARCH_QUERY_TEXT_Y);
    lv_obj_set_width(
        label,
        CRAZYPOD_METRIC_SEARCH_QUERY_WIDTH -
            CRAZYPOD_METRIC_SEARCH_QUERY_TEXT_X -
            CRAZYPOD_METRIC_SEARCH_QUERY_TEXT_TRAIL);
    lv_obj_set_height(
        label, CRAZYPOD_METRIC_SEARCH_QUERY_TEXT_HEIGHT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
#if CRAZYPOD_METRIC_SEARCH_SHOW_COUNT
    /* The count the wide layout prints over its results preview. */
    snprintf(text, sizeof(text), CP_FMT("%d match%s"),
             result_count, result_count == 1 ? "" : "es");
#ifdef HAVE_CRAZYPOD_MONO_UI
    label = crazypod_ui_widget_label_shade(
        query_box, context->query[0] != '\0' ? text : "",
        &lv_font_montserrat_8, QUERY_TEXT, LV_OPA_COVER);
#else
    label = make_label(query_box,
                       context->query[0] != '\0' ? text : "",
                       &lv_font_montserrat_8, COLOR_WHITE, 205);
#endif
    lv_obj_set_pos(
        label,
        CRAZYPOD_METRIC_SEARCH_QUERY_WIDTH -
            CRAZYPOD_METRIC_SEARCH_COUNT_BACK,
        CRAZYPOD_METRIC_SEARCH_QUERY_TEXT_Y + 1);
    lv_obj_set_width(label, CRAZYPOD_METRIC_SEARCH_COUNT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
#endif

    start = crazypod_ui_menu_window_start(
        count, state->selected, CRAZYPOD_VISIBLE_ROWS);

    for(row = 0; row < CRAZYPOD_VISIBLE_ROWS; ++row) {
        int index = start + row;
        int y = CRAZYPOD_MENU_ROW_Y + row * CRAZYPOD_MENU_ROW_STEP;
        bool selected = index == state->selected;
        lv_obj_t *row_box;
        lv_obj_t *marker;
        const char *title;
#ifdef HAVE_CRAZYPOD_MONO_UI
        uint32_t text_color = selected ? ROW_SELECTED_TEXT : ROW_TEXT;
#endif

        if(index >= count)
            break;

#ifdef HAVE_CRAZYPOD_MONO_UI
        row_box = crazypod_ui_widget_box_shade(
            context->parent,
            CRAZYPOD_MENU_ROW_X, y,
            CRAZYPOD_MENU_ROW_WIDTH,
            CRAZYPOD_MENU_ROW_HEIGHT,
            CRAZYPOD_METRIC_MENU_ROW_RADIUS,
            ROW_SELECTED_FILL,
            selected ? LV_OPA_COVER : LV_OPA_TRANSP);
#else
        row_box = make_box(context->parent,
                           CRAZYPOD_MENU_ROW_X, y,
                           CRAZYPOD_MENU_ROW_WIDTH,
                           CRAZYPOD_MENU_ROW_HEIGHT, 8,
                           selected ? context->primary_color : context->panel_color,
                           selected ? 220 : LV_OPA_TRANSP);
        if(selected) {
            if(context->gradient_highlight) {
                lv_obj_set_style_bg_grad_color(
                    row_box, crazypod_ui_color(context->secondary_color), 0);
                lv_obj_set_style_bg_grad_dir(row_box, LV_GRAD_DIR_HOR, 0);
            }
            lv_obj_set_style_border_width(row_box, 1, 0);
            lv_obj_set_style_border_color(row_box,
                                           crazypod_ui_color(COLOR_WHITE), 0);
            lv_obj_set_style_border_opa(row_box, 90, 0);
        }
#endif

        title = context->item_title(state, index);
#ifdef HAVE_CRAZYPOD_MONO_UI
        label = crazypod_ui_widget_label_shade(
            row_box, title != NULL ? title : "",
            context->metadata_font, text_color, LV_OPA_COVER);
#else
        label = make_label(row_box, title != NULL ? title : "",
                           context->metadata_font,
                           COLOR_WHITE,
                           selected ? 255 : 150);
#endif
        lv_obj_set_width(
            label,
            CRAZYPOD_MENU_ROW_WIDTH - CRAZYPOD_METRIC_ROW_TEXT_X - 22);
        crazypod_ui_widget_align_row_label(
            label, CRAZYPOD_METRIC_ROW_TEXT_X,
            CRAZYPOD_UI_ROW_LABEL_TEXT);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

#ifdef HAVE_CRAZYPOD_MONO_UI
        marker = crazypod_ui_widget_label_shade(
            row_box, selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET,
            &lv_font_montserrat_8,
            selected ? ROW_SELECTED_TEXT : MARKER_TEXT, LV_OPA_COVER);
#else
        marker = make_label(row_box,
                            selected ? LV_SYMBOL_PLAY : "",
                            &lv_font_montserrat_8,
                            COLOR_WHITE, selected ? 205 : 75);
#endif
        crazypod_ui_widget_align_row_label(
            marker, CRAZYPOD_METRIC_ROW_MARKER_X,
            CRAZYPOD_UI_ROW_LABEL_MARKER);
        crazypod_menu_list_bind_row(row, row_box, label, marker);
    }

#if CRAZYPOD_METRIC_SEARCH_SHOW_RESULTS
    context->make_panel(
        context->parent, CRAZYPOD_GLASS_SLOT_SEARCH_RESULTS,
        170, 91, 136, 104, 12);
    snprintf(text, sizeof(text), CP_FMT("%d match%s"),
             result_count, result_count == 1 ? "" : "es");
    label = make_label(context->parent,
                       context->query[0] != '\0'
                           ? text : CP_TR("Live results"),
                       &lv_font_montserrat_10,
                       COLOR_WHITE,
                       context->query[0] != '\0' ? 205 : 105);
    lv_obj_set_pos(label, 182, 101);
    lv_obj_set_width(label, 112);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    if(context->query[0] == '\0') {
        label = make_label(context->parent,
                           CP_TR("Choose a letter, then press Select."),
                           &lv_font_montserrat_8,
                           COLOR_WHITE, 100);
        lv_obj_set_pos(label, 182, 125);
        lv_obj_set_width(label, 112);
        lv_obj_set_height(label, 40);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    }
    else if(result_count <= 0) {
        label = make_label(context->parent,
                           context->empty_hint,
                           &lv_font_montserrat_8,
                           COLOR_WHITE, 105);
        lv_obj_set_pos(label, 182, 125);
        lv_obj_set_width(label, 112);
        lv_obj_set_height(label, 45);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    }
    else {
        int shown = result_count < CRAZYPOD_SEARCH_PREVIEW_ROWS
            ? result_count : CRAZYPOD_SEARCH_PREVIEW_ROWS;
        const char *subtitle;
        int i;
        for(i = 0; i < shown; ++i) {
            int y = 124 + i * 30;
            const char *title =
                context->result_title(context->query, i);

            if(title == NULL)
                continue;
            label = make_label(context->parent, title,
                               &lv_font_montserrat_8,
                               COLOR_WHITE, i == 0 ? 210 : 145);
            lv_obj_set_pos(label, 182, y);
            lv_obj_set_width(label, 112);
            lv_obj_set_height(label, 15);
            lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
            subtitle = context->result_subtitle != NULL
                ? context->result_subtitle(context->query, i) : NULL;
            if(subtitle == NULL)
                continue;
            label = make_label(context->parent, subtitle,
                               &lv_font_montserrat_8,
                               COLOR_WHITE, 75);
            lv_obj_set_pos(label, 182, y + 15);
            lv_obj_set_width(label, 112);
            lv_obj_set_height(label, 15);
            lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        }
    }
#endif

#if CRAZYPOD_METRIC_SEARCH_SHOW_HINTS
    label = make_label(context->parent,
                       CP_TR("Wheel Choose  Select Action"),
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 125);
    lv_obj_set_pos(label, 174, 200);
    lv_obj_set_width(label, 128);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    label = make_label(context->parent,
                       CP_TR("Choose View Results to listen"),
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 95);
    lv_obj_set_pos(label, 174, 216);
    lv_obj_set_width(label, 128);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
#endif
}



#endif
