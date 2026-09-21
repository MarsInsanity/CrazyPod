#include "config.h"

#ifdef HAVE_CRAZYPOD_UI

#include "crazypod_empty_state.h"
#include "crazypod_overlay_glass.h"
#include "crazypod_popup_layout.h"
#include "crazypod_ui_widgets.h"

#define COLOR_WHITE 0xFFFFFF
#ifdef HAVE_CRAZYPOD_COMPACT_UI
/*
 * A 24px symbol over a 168px minimum card is most of this panel, and the
 * message under it was set at a half opacity that has no shade here: on
 * four steps it landed on the card it was printed on, so "Add local music
 * and rescan" was there but could not be read.
 */
#define EMPTY_SYMBOL_FONT (&lv_font_montserrat_16)
#define EMPTY_SYMBOL_STEP 22
#define EMPTY_SYMBOL_OPA LV_OPA_COVER
#define EMPTY_TITLE_FONT (&lv_font_montserrat_12)
#define EMPTY_MESSAGE_OPA LV_OPA_COVER
#define EMPTY_INSET 5
#define EMPTY_PADDING 6
#define EMPTY_MIN_WIDTH 0
#define EMPTY_MAX_WIDTH (LCD_WIDTH - 8)
#else
#define EMPTY_SYMBOL_FONT (&lv_font_montserrat_24)
#define EMPTY_SYMBOL_STEP 38
#define EMPTY_SYMBOL_OPA 155
#define EMPTY_TITLE_FONT (&lv_font_montserrat_12)
#define EMPTY_MESSAGE_OPA 135
#define EMPTY_INSET 14
#define EMPTY_PADDING 16
#define EMPTY_MIN_WIDTH 168
#define EMPTY_MAX_WIDTH (LCD_WIDTH - 32)
#endif

void crazypod_empty_state_render(
    lv_obj_t *parent, const char *symbol,
    const char *title, const char *message)
{
    bool has_symbol = symbol != NULL && symbol[0] != '\0';
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *label;
    struct crazypod_popup_geometry geometry;
    int content_width;
    int measured_width;
    int message_height;
    int y;

    if(parent == NULL || title == NULL || message == NULL)
        return;
    measured_width = crazypod_popup_text_width(
        title, EMPTY_TITLE_FONT);
    content_width = crazypod_popup_text_width(
        message, &lv_font_montserrat_8);
    if(content_width > measured_width)
        measured_width = content_width;
    geometry = crazypod_popup_centered_geometry(
        crazypod_popup_clamp_width(
            measured_width, EMPTY_INSET + 4,
            EMPTY_MIN_WIDTH, EMPTY_MAX_WIDTH),
        1);
    content_width = geometry.width - 2 * EMPTY_INSET;
    message_height = crazypod_popup_wrapped_text_height(
        message, &lv_font_montserrat_8,
        content_width, 2);
    geometry = crazypod_popup_centered_geometry(
        geometry.width,
        EMPTY_PADDING + (has_symbol ? EMPTY_SYMBOL_STEP : 0) +
        lv_font_get_line_height(EMPTY_TITLE_FONT) +
        8 + message_height + EMPTY_PADDING);
    root = crazypod_ui_widget_box(
        parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, LV_OPA_TRANSP);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_CLICKABLE);
    crazypod_overlay_glass_prepare(true);
    panel = crazypod_overlay_glass_panel(
        root, geometry.x, geometry.y,
        geometry.width, geometry.height);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    y = EMPTY_PADDING;
    if(has_symbol) {
        label = crazypod_ui_widget_label(
            panel, symbol, EMPTY_SYMBOL_FONT,
            COLOR_WHITE, EMPTY_SYMBOL_OPA);
        lv_obj_set_width(label, geometry.width);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 0, y);
        y += EMPTY_SYMBOL_STEP;
    }
    label = crazypod_ui_widget_label(
        panel, title, EMPTY_TITLE_FONT,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, content_width);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, EMPTY_INSET, y);
    y += lv_font_get_line_height(EMPTY_TITLE_FONT) + 8;
    label = crazypod_ui_widget_label(
        panel, message, &lv_font_montserrat_8,
        COLOR_WHITE, EMPTY_MESSAGE_OPA);
    lv_obj_set_width(label, content_width);
    lv_obj_set_height(label, message_height);
    lv_obj_set_style_text_align(
        label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_line_space(label, 2, 0);
    lv_obj_set_pos(label, EMPTY_INSET, y);
}

#endif
