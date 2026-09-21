#include "config.h"

#include "../../../crazypod_l10n.h"

#include <string.h>

#include "lvgl.h"

#include "../../../crazypod_organizer.h"
#include "../../presentation/crazypod_ui_text.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "../../../crazypod_state.h"
#include "crazypod_contacts_screen.h"
#include "../../../crazypod_color.h"

#include "../../presentation/crazypod_ui_metrics.h"
#include "../../../crazypod_runtime_font.h"

#define CRAZYPOD_CONTACT_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_CONTACT_WHITE 0xFFFFFF

#ifdef HAVE_CRAZYPOD_COMPACT_UI
/*
 * A 180x184 card at 70,42 starts past the middle of this panel and ends
 * well below it, so the Mini showed a sliver of its left edge and nothing
 * else. Here the card is nearly the whole screen, the avatar is a third
 * of the size, and the two rows are the height of a menu row.
 */
#define CONTACT_CARD_X 3
#define CONTACT_CARD_Y (CRAZYPOD_METRIC_STATUS_HEIGHT + 2)
#define CONTACT_CARD_WIDTH (LCD_WIDTH - 6)
#define CONTACT_CARD_HEIGHT (LCD_HEIGHT - CONTACT_CARD_Y - 3)
#define CONTACT_CARD_RADIUS 6
#define CONTACT_AVATAR_SIZE 30
#define CONTACT_AVATAR_Y 4
#define CONTACT_NAME_FONT (crazypod_runtime_font_at_size(12))
#define CONTACT_NAME_Y 36
#define CONTACT_INSET 5
#define CONTACT_ROW_Y 54
#define CONTACT_ROW_STEP 19
#define CONTACT_ROW_HEIGHT 17
#define CONTACT_ROW_RADIUS 5
#define CONTACT_ROW_TEXT_HEIGHT 11
#define CONTACT_ROW_ICON_X 4
#define CONTACT_ROW_ICON_Y 4
#define CONTACT_ROW_TEXT_X 18
#define CONTACT_ROW_TEXT_Y 3
#define CONTACT_ROW_FONT (&lv_font_montserrat_8)
#else
#define CONTACT_CARD_X 70
#define CONTACT_CARD_Y 42
#define CONTACT_CARD_WIDTH 180
#define CONTACT_CARD_HEIGHT 184
#define CONTACT_CARD_RADIUS 18
#define CONTACT_AVATAR_SIZE 54
#define CONTACT_AVATAR_Y 14
#define CONTACT_NAME_FONT CRAZYPOD_CONTACT_FONT
#define CONTACT_NAME_Y 76
#define CONTACT_INSET 12
#define CONTACT_ROW_Y 103
#define CONTACT_ROW_STEP 37
#define CONTACT_ROW_HEIGHT 30
#define CONTACT_ROW_RADIUS 8
#define CONTACT_ROW_TEXT_HEIGHT 19
#define CONTACT_ROW_ICON_X 10
#define CONTACT_ROW_ICON_Y 9
#define CONTACT_ROW_TEXT_X 31
#define CONTACT_ROW_TEXT_Y 8
#define CONTACT_ROW_FONT (&lv_font_montserrat_10)
#endif
#define CONTACT_ROW_WIDTH (CONTACT_CARD_WIDTH - 2 * CONTACT_INSET)
#define CONTACT_ROW_TEXT_WIDTH \
    (CONTACT_ROW_WIDTH - CONTACT_ROW_TEXT_X - 7)

void crazypod_contacts_screen_render(lv_obj_t *content, int contact_index)
{
    const struct crazypod_contact *contact =
        crazypod_contact_get(contact_index);
    lv_obj_t *card;
    lv_obj_t *avatar;
    lv_obj_t *label;
    lv_obj_t *row;
    char initials[8];
    int initial_bytes = 0;

    initials[0] = '?';
    initials[1] = '\0';
    if(contact != NULL && contact->name[0] != '\0') {
        initial_bytes = crazypod_ui_text_character_size(contact->name);
        if(initial_bytes > 0 &&
           initial_bytes < (int)sizeof(initials)) {
            memcpy(initials, contact->name,
                   (size_t)initial_bytes);
            initials[initial_bytes] = '\0';
        }
    }

    crazypod_ui_widget_box(
        content, 0, CRAZYPOD_METRIC_STATUS_HEIGHT, LCD_WIDTH,
        LCD_HEIGHT - CRAZYPOD_METRIC_STATUS_HEIGHT, 0,
        0x000000, 105);
    card = crazypod_ui_widget_box(
        content, CONTACT_CARD_X, CONTACT_CARD_Y,
        CONTACT_CARD_WIDTH, CONTACT_CARD_HEIGHT,
        CONTACT_CARD_RADIUS, 0x242A31, 238);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, crazypod_ui_color(CRAZYPOD_CONTACT_WHITE), 0);
    lv_obj_set_style_border_opa(card, 42, 0);
    if(!crazypod_state_reduce_effects()) {
        lv_obj_set_style_shadow_width(card, 18, 0);
        lv_obj_set_style_shadow_offset_y(card, 10, 0);
        lv_obj_set_style_shadow_color(card, crazypod_ui_color(0x000000), 0);
        lv_obj_set_style_shadow_opa(card, 95, 0);
    }

    avatar = crazypod_ui_widget_box(
        card, (CONTACT_CARD_WIDTH - CONTACT_AVATAR_SIZE) / 2,
        CONTACT_AVATAR_Y, CONTACT_AVATAR_SIZE, CONTACT_AVATAR_SIZE,
        LV_RADIUS_CIRCLE, 0x59B89E, LV_OPA_COVER);
    lv_obj_set_style_bg_grad_color(
        avatar, crazypod_ui_color(0x2E4857), 0);
    lv_obj_set_style_bg_grad_dir(avatar, LV_GRAD_DIR_VER, 0);
    label = crazypod_ui_widget_label(avatar, initials,
                       CONTACT_NAME_FONT,
                       CRAZYPOD_CONTACT_WHITE, LV_OPA_COVER);
    lv_obj_center(label);
    label = crazypod_ui_widget_label(
        card,
        contact != NULL ? contact->name : CP_TR("Missing Contact"),
        CONTACT_NAME_FONT, CRAZYPOD_CONTACT_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, CONTACT_ROW_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, CONTACT_INSET, CONTACT_NAME_Y);

    row = crazypod_ui_widget_box(
        card, CONTACT_INSET, CONTACT_ROW_Y,
        CONTACT_ROW_WIDTH, CONTACT_ROW_HEIGHT, CONTACT_ROW_RADIUS,
        CRAZYPOD_CONTACT_WHITE, 28);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, crazypod_ui_color(CRAZYPOD_CONTACT_WHITE), 0);
    lv_obj_set_style_border_opa(row, 32, 0);
    label = crazypod_ui_widget_label(row, LV_SYMBOL_CALL,
                       CONTACT_ROW_FONT,
                       CRAZYPOD_CONTACT_WHITE, 175);
    lv_obj_set_pos(label, CONTACT_ROW_ICON_X, CONTACT_ROW_ICON_Y);
    label = crazypod_ui_widget_label(
        row,
        contact != NULL && contact->phone[0] != '\0'
            ? contact->phone : CP_TR("No phone number"),
        CONTACT_ROW_FONT, CRAZYPOD_CONTACT_WHITE,
        contact != NULL && contact->phone[0] != '\0'
            ? 225 : 105);
    lv_obj_set_width(label, CONTACT_ROW_TEXT_WIDTH);
    lv_obj_set_height(label, CONTACT_ROW_TEXT_HEIGHT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, CONTACT_ROW_TEXT_X, CONTACT_ROW_TEXT_Y);

    row = crazypod_ui_widget_box(
        card, CONTACT_INSET, CONTACT_ROW_Y + CONTACT_ROW_STEP,
        CONTACT_ROW_WIDTH, CONTACT_ROW_HEIGHT, CONTACT_ROW_RADIUS,
        CRAZYPOD_CONTACT_WHITE, 16);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, crazypod_ui_color(CRAZYPOD_CONTACT_WHITE), 0);
    lv_obj_set_style_border_opa(row, 22, 0);
    label = crazypod_ui_widget_label(row, LV_SYMBOL_ENVELOPE,
                       CONTACT_ROW_FONT,
                       CRAZYPOD_CONTACT_WHITE, 135);
    lv_obj_set_pos(label, CONTACT_ROW_ICON_X, CONTACT_ROW_ICON_Y);
    label = crazypod_ui_widget_label(
        row,
        contact != NULL && contact->email[0] != '\0'
            ? contact->email : CP_TR("No email address"),
        CONTACT_ROW_FONT, CRAZYPOD_CONTACT_WHITE,
        contact != NULL && contact->email[0] != '\0'
            ? 190 : 90);
    lv_obj_set_width(label, CONTACT_ROW_TEXT_WIDTH);
    lv_obj_set_height(label, CONTACT_ROW_TEXT_HEIGHT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, CONTACT_ROW_TEXT_X, CONTACT_ROW_TEXT_Y);
}
