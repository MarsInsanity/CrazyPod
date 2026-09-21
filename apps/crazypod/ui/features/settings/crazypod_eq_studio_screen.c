#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include <stdio.h>

#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_eq_studio_screen.h"
#include "../../../crazypod_color.h"
#include "../../../crazypod_mono.h"

#define WHITE 0xFFFFFF
#define MUTED 0x8E8E93
#define GREEN 0x30D158
#define ROSE 0xFF375F
#define CYAN 0x26CFF5
#define AMBER 0xFFB340
#define GAIN_MAX 240

#ifdef HAVE_CRAZYPOD_COMPACT_UI
/*
 * Ten bands at 27px apart is 270 pixels of graph, a 44px footer at 174 and
 * a row of hints at 225: on this panel the Mini showed the title, the
 * readout and nothing else, because the rest of the screen was past the
 * right and bottom edges.
 *
 * What goes: the per-band frequency captions, which would be thirteen
 * pixels wide each and the footer names the selected one anyway; the "0
 * dB" tick, which the three rules already say; the mode caption, which the
 * chips below it already show; and the key hints, which are three lines of
 * text for controls there is no room to describe.
 */
#define EQ_HEADER_Y 12
#define EQ_HEADER_HEIGHT 14
#define EQ_TITLE_X 3
#define EQ_TITLE_Y 13
#define EQ_TITLE_WIDTH 52
#define EQ_STATE_X 55
#define EQ_STATE_Y 15
#define EQ_STATE_WIDTH 38
#define EQ_EDIT_X 94
#define EQ_EDIT_Y 15
#define EQ_EDIT_WIDTH 40
#define EQ_READOUT_SHOW_FREQUENCY 0
#define EQ_READOUT_X 3
#define EQ_READOUT_Y 28
#define EQ_READOUT_WIDTH 62
#define EQ_PRECUT_X 66
#define EQ_PRECUT_Y 29
#define EQ_PRECUT_WIDTH 68
#define EQ_RULE_X 3
#define EQ_RULE_WIDTH (LCD_WIDTH - 6)
#define EQ_TOP_RULE_Y 41
#define EQ_ZERO_RULE_Y 60
#define EQ_BOTTOM_RULE_Y 78
#define EQ_SPAN 18
#define EQ_BAND_X0 9
#define EQ_BAND_STEP 13
#define EQ_BAR_WIDTH 7
#define EQ_BAR_SELECTED_WIDTH 11
#define EQ_BAR_RADIUS 2
#define EQ_SHOW_ZERO_LABEL 0
#define EQ_SHOW_BAND_LABELS 0
#define EQ_BAND_LABEL_Y 0
#define EQ_FOOTER_Y 81
#define EQ_FOOTER_HEIGHT (LCD_HEIGHT - 81)
#define EQ_BAND_FONT (&lv_font_montserrat_12)
#define EQ_BAND_X 3
#define EQ_BAND_Y 82
#define EQ_BAND_WIDTH 32
#define EQ_ROLE_X 36
#define EQ_ROLE_Y 85
#define EQ_ROLE_WIDTH 99
#define EQ_SHOW_MODE 0
#define EQ_SHOW_WHEEL_HINT 0
#define EQ_CHIP_Y 96
#define EQ_CHIP_WIDTH 32
#define EQ_CHIP_HEIGHT 12
#define EQ_CHIP_STEP 34
#define EQ_CHIP_X0 2
#define EQ_CHIP_TEXT_Y 1
#define EQ_SHOW_KEY_HINTS 0
#else
#define EQ_HEADER_Y 29
#define EQ_HEADER_HEIGHT 35
#define EQ_TITLE_X 14
#define EQ_TITLE_Y 39
#define EQ_TITLE_WIDTH 120
#define EQ_STATE_X 190
#define EQ_STATE_Y 41
#define EQ_STATE_WIDTH 60
#define EQ_EDIT_X 252
#define EQ_EDIT_Y 42
#define EQ_EDIT_WIDTH 54
#define EQ_READOUT_SHOW_FREQUENCY 1
#define EQ_READOUT_X 14
#define EQ_READOUT_Y 66
#define EQ_READOUT_WIDTH 198
#define EQ_PRECUT_X 216
#define EQ_PRECUT_Y 68
#define EQ_PRECUT_WIDTH 90
#define EQ_RULE_X 14
#define EQ_RULE_WIDTH 292
#define EQ_TOP_RULE_Y 84
#define EQ_ZERO_RULE_Y 124
#define EQ_BOTTOM_RULE_Y 163
#define EQ_SPAN 38
#define EQ_BAND_X0 29
#define EQ_BAND_STEP 27
#define EQ_BAR_WIDTH 10
#define EQ_BAR_SELECTED_WIDTH 16
#define EQ_BAR_RADIUS 4
#define EQ_SHOW_ZERO_LABEL 1
#define EQ_SHOW_BAND_LABELS 1
#define EQ_BAND_LABEL_Y 159
#define EQ_FOOTER_Y 174
#define EQ_FOOTER_HEIGHT 44
#define EQ_BAND_FONT (&lv_font_montserrat_16)
#define EQ_BAND_X 14
#define EQ_BAND_Y 175
#define EQ_BAND_WIDTH 44
#define EQ_ROLE_X 62
#define EQ_ROLE_Y 177
#define EQ_ROLE_WIDTH 75
#define EQ_SHOW_MODE 1
#define EQ_SHOW_WHEEL_HINT 1
#define EQ_CHIP_Y 196
#define EQ_CHIP_WIDTH 58
#define EQ_CHIP_HEIGHT 21
#define EQ_CHIP_STEP 66
#define EQ_CHIP_X0 14
#define EQ_CHIP_TEXT_Y 3
#define EQ_SHOW_KEY_HINTS 1
#endif

/*
 * On the monochrome panel these labels sit on a pale band, and the design
 * separates them by opacity -- 245 for the title, 125 for the mode, 120
 * for the band role. Four shades have no such steps, so the shade is named
 * here and the colour build keeps the colour and the opacity it had.
 *
 * Only text on the page is set a step back. On the pale bands a step is
 * all the contrast there is, and eight pixel type at one step reads as a
 * smudge, so everything printed on a band is ink.
 */
#define EQ_TEXT_STRONG CRAZYPOD_MONO_INK
#define EQ_TEXT_BAND CRAZYPOD_MONO_INK
#define EQ_TEXT_SOFT CRAZYPOD_MONO_SHADE_DARK

static lv_obj_t *eq_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    uint32_t color, lv_opa_t opacity, uint32_t shade)
{
#ifdef HAVE_CRAZYPOD_MONO_UI
    (void)color;
    (void)opacity;
    return crazypod_ui_widget_label_shade(
        parent, text, font, shade, LV_OPA_COVER);
#else
    (void)shade;
    return crazypod_ui_widget_label(
        parent, text, font, color, opacity);
#endif
}

static void chip(
    lv_obj_t *parent, int x, const char *title,
    bool active, uint32_t primary_color)
{
#ifdef HAVE_CRAZYPOD_MONO_UI
    /*
     * An active chip at four fifths opacity and an idle one at a twelfth
     * are the same shade here, so the chip is filled or it is not, and the
     * title is knocked out of the fill the way a selected menu row is.
     */
    lv_obj_t *box = crazypod_ui_widget_box_shade(
        parent, x, EQ_CHIP_Y, EQ_CHIP_WIDTH, EQ_CHIP_HEIGHT, 5,
        active ? CRAZYPOD_MONO_INK : CRAZYPOD_MONO_SHADE_PALE,
        LV_OPA_COVER);
    lv_obj_t *label = crazypod_ui_widget_label_shade(
        box, title, &lv_font_montserrat_8,
        active ? CRAZYPOD_MONO_PAPER : CRAZYPOD_MONO_INK,
        LV_OPA_COVER);

    (void)primary_color;
#else
    lv_obj_t *box = crazypod_ui_widget_box(
        parent, x, EQ_CHIP_Y, EQ_CHIP_WIDTH, EQ_CHIP_HEIGHT, 9,
        active ? primary_color : WHITE, active ? 210 : 20);
    lv_obj_t *label = crazypod_ui_widget_label(
        box, title, &lv_font_montserrat_8,
        WHITE, active ? 255 : 140);
#endif

    lv_obj_set_width(label, EQ_CHIP_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 0, EQ_CHIP_TEXT_Y);
}

static int bar_y(int gain)
{
    return EQ_ZERO_RULE_Y - gain * EQ_SPAN / GAIN_MAX;
}

void crazypod_eq_studio_screen_render(
    lv_obj_t *parent,
    const struct crazypod_eq_studio_model *model,
    const lv_font_t *metadata_font,
    uint32_t primary_color)
{
    static const char *const fixed_labels[EQ_NUM_BANDS] = {
        "32", "64", "125", "250", "500",
        CP_TR("1k"), CP_TR("2k"), CP_TR("4k"), CP_TR("8k"), CP_TR("16k")
    };
    const struct eq_band_setting *current =
        &model->bands[model->band];
    lv_obj_t *label;
    char text[96];
    char gain_text[24];
    char frequency_text[24];
    char q_text[24];
    char precut_text[24];
    int max_gain = 0;
    int index;
    bool clipping_risk;

    crazypod_ui_widget_box(
        parent, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0,
        0x050508, LV_OPA_COVER);
#ifdef HAVE_CRAZYPOD_MONO_UI
    /*
     * The header and footer bands are near-black on a near-black page,
     * which is a separation the design map turns into paper on paper. A
     * pale band over the page says the same thing in shades this panel
     * has.
     */
    crazypod_ui_widget_box_shade(
        parent, 0, EQ_HEADER_Y, LCD_WIDTH, EQ_HEADER_HEIGHT, 0,
        CRAZYPOD_MONO_SHADE_PALE, LV_OPA_COVER);
#else
    crazypod_ui_widget_box(
        parent, 0, EQ_HEADER_Y, LCD_WIDTH, EQ_HEADER_HEIGHT, 0,
        0x101017, 235);
#endif
    label = eq_label(
        parent, CP_TR("EQ Studio"), metadata_font, WHITE, 245,
        EQ_TEXT_STRONG);
    lv_obj_set_pos(label, EQ_TITLE_X, EQ_TITLE_Y);
    lv_obj_set_width(label, EQ_TITLE_WIDTH);
    label = eq_label(
        parent, model->enabled ? CP_TR("On") : CP_TR("Bypass"),
        &lv_font_montserrat_10,
        model->enabled ? GREEN : MUTED, 240, EQ_TEXT_BAND);
    lv_obj_set_width(label, EQ_STATE_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, EQ_STATE_X, EQ_STATE_Y);
    label = eq_label(
        parent, model->editing ? CP_TR("EDIT") : CP_TR("BROWSE"),
        &lv_font_montserrat_8, WHITE, 125, EQ_TEXT_BAND);
    lv_obj_set_width(label, EQ_EDIT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, EQ_EDIT_X, EQ_EDIT_Y);

    crazypod_eq_studio_format_db(
        gain_text, sizeof(gain_text), current->gain);
    crazypod_eq_studio_format_frequency(
        frequency_text, sizeof(frequency_text), current->cutoff);
    crazypod_eq_studio_format_q(
        q_text, sizeof(q_text), current->q);
    crazypod_eq_studio_format_precut(
        precut_text, sizeof(precut_text), model->precut);
#if EQ_READOUT_SHOW_FREQUENCY
    snprintf(text, sizeof(text), "%s  %s  %s",
             frequency_text, gain_text, q_text);
#else
    /* The footer already names the band, so the frequency is not
     * repeated here; the room it wanted is the room Precut needs. */
    (void)frequency_text;
    snprintf(text, sizeof(text), "%s  %s", gain_text, q_text);
#endif
    label = eq_label(
        parent, text, &lv_font_montserrat_10, WHITE, 180,
        EQ_TEXT_STRONG);
    lv_obj_set_pos(label, EQ_READOUT_X, EQ_READOUT_Y);
    lv_obj_set_width(label, EQ_READOUT_WIDTH);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    for(index = 0; index < EQ_NUM_BANDS; ++index) {
        if(model->bands[index].gain > max_gain)
            max_gain = model->bands[index].gain;
    }
    clipping_risk = max_gain > 0 && max_gain > model->precut;
    snprintf(text, sizeof(text), CP_FMT("Precut %s"), precut_text);
    label = eq_label(
        parent, text, &lv_font_montserrat_8,
        clipping_risk ? AMBER : WHITE,
        clipping_risk ? 235 : 125,
        clipping_risk ? EQ_TEXT_STRONG : EQ_TEXT_SOFT);
    lv_obj_set_width(label, EQ_PRECUT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, EQ_PRECUT_X, EQ_PRECUT_Y);

#ifdef HAVE_CRAZYPOD_MONO_UI
    crazypod_ui_widget_box_shade(
        parent, EQ_RULE_X, EQ_ZERO_RULE_Y, EQ_RULE_WIDTH, 1, 0,
        CRAZYPOD_MONO_SHADE_DARK, LV_OPA_COVER);
    crazypod_ui_widget_box_shade(
        parent, EQ_RULE_X, EQ_TOP_RULE_Y, EQ_RULE_WIDTH, 1, 0,
        CRAZYPOD_MONO_SHADE_PALE, LV_OPA_COVER);
    crazypod_ui_widget_box_shade(
        parent, EQ_RULE_X, EQ_BOTTOM_RULE_Y, EQ_RULE_WIDTH, 1, 0,
        CRAZYPOD_MONO_SHADE_PALE, LV_OPA_COVER);
#else
    crazypod_ui_widget_box(
        parent, EQ_RULE_X, EQ_ZERO_RULE_Y, EQ_RULE_WIDTH, 1, 0,
        WHITE, 70);
    crazypod_ui_widget_box(
        parent, EQ_RULE_X, EQ_TOP_RULE_Y, EQ_RULE_WIDTH, 1, 0,
        WHITE, 18);
    crazypod_ui_widget_box(
        parent, EQ_RULE_X, EQ_BOTTOM_RULE_Y, EQ_RULE_WIDTH, 1, 0,
        WHITE, 18);
#endif
#if EQ_SHOW_ZERO_LABEL
    label = crazypod_ui_widget_label(
        parent, CP_TR("0 dB"), &lv_font_montserrat_8, WHITE, 95);
    lv_obj_set_pos(label, 16, 106);
#endif

    for(index = 0; index < EQ_NUM_BANDS; ++index) {
        int gain = model->bands[index].gain;
        int absolute = gain < 0 ? -gain : gain;
        int height = absolute * EQ_SPAN / GAIN_MAX;
        int x = EQ_BAND_X0 + index * EQ_BAND_STEP;
        int y = gain >= 0
            ? EQ_ZERO_RULE_Y - height : EQ_ZERO_RULE_Y + 1;
        int width = index == model->band
            ? EQ_BAR_SELECTED_WIDTH : EQ_BAR_WIDTH;
#ifndef HAVE_CRAZYPOD_MONO_UI
        uint32_t color = gain >= 0 ? GREEN : ROSE;
        lv_opa_t opacity = model->enabled ? 230 : 80;
#endif
        lv_obj_t *bar;

        if(height < 2)
            height = 2;
#ifdef HAVE_CRAZYPOD_MONO_UI
        /*
         * Boost green and cut rose are one shade apart from each other and
         * from the accent the selected bar is painted in, so the graph was
         * ten identical grey sticks. Which bar is selected is what the
         * wheel is acting on, so that one is solid ink and the rest are a
         * step back; bypassed, the whole graph goes pale.
         */
        bar = crazypod_ui_widget_box_shade(
            parent, x - width / 2, y, width, height,
            EQ_BAR_RADIUS,
            !model->enabled ? CRAZYPOD_MONO_SHADE_PALE
                : index == model->band ? CRAZYPOD_MONO_INK
                    : CRAZYPOD_MONO_SHADE_DARK,
            LV_OPA_COVER);
        crazypod_ui_widget_box_shade(
            parent, x - 2, bar_y(gain) - 2, 4, 4,
            LV_RADIUS_CIRCLE,
            !model->enabled ? CRAZYPOD_MONO_SHADE_PALE
                : CRAZYPOD_MONO_INK,
            LV_OPA_COVER);
        (void)bar;
        (void)primary_color;
#else
        if(index == model->band)
            color = primary_color;
        bar = crazypod_ui_widget_box(
            parent, x - width / 2, y, width, height,
            EQ_BAR_RADIUS, color, opacity);
        if(index == model->band) {
            lv_obj_set_style_border_width(bar, 1, 0);
            lv_obj_set_style_border_color(
                bar, crazypod_ui_color(WHITE), 0);
            lv_obj_set_style_border_opa(bar, 95, 0);
        }
        crazypod_ui_widget_box(
            parent, x - 2, bar_y(gain) - 2, 4, 4,
            LV_RADIUS_CIRCLE, color, opacity);
#endif
#if EQ_SHOW_BAND_LABELS
        label = crazypod_ui_widget_label(
            parent, fixed_labels[index], &lv_font_montserrat_8,
            index == model->band ? WHITE : MUTED,
            index == model->band ? 235 : 110);
        lv_obj_set_width(label, 28);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, x - 14, EQ_BAND_LABEL_Y);
#endif
    }

#ifdef HAVE_CRAZYPOD_MONO_UI
    crazypod_ui_widget_box_shade(
        parent, 0, EQ_FOOTER_Y, LCD_WIDTH, EQ_FOOTER_HEIGHT, 0,
        CRAZYPOD_MONO_SHADE_PALE, LV_OPA_COVER);
#else
    crazypod_ui_widget_box(
        parent, 0, EQ_FOOTER_Y, LCD_WIDTH, EQ_FOOTER_HEIGHT, 0,
        0x111119, 235);
#endif
    label = eq_label(
        parent, fixed_labels[model->band],
        EQ_BAND_FONT, WHITE, 245, EQ_TEXT_STRONG);
    lv_obj_set_pos(label, EQ_BAND_X, EQ_BAND_Y);
    lv_obj_set_width(label, EQ_BAND_WIDTH);
    label = eq_label(
        parent, crazypod_eq_studio_band_role(model->band),
        &lv_font_montserrat_8, WHITE, 120, EQ_TEXT_BAND);
    lv_obj_set_pos(label, EQ_ROLE_X, EQ_ROLE_Y);
    lv_obj_set_width(label, EQ_ROLE_WIDTH);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
#if EQ_SHOW_MODE
    label = crazypod_ui_widget_label(
        parent, crazypod_eq_studio_mode_title(model->mode),
        &lv_font_montserrat_10, CYAN, 225);
    lv_obj_set_pos(label, 142, 176);
    lv_obj_set_width(label, 60);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
#endif
#if EQ_SHOW_WHEEL_HINT
    label = crazypod_ui_widget_label(
        parent,
        model->editing ? CP_TR("Wheel adjusts") : CP_TR("Wheel selects"),
        &lv_font_montserrat_8, WHITE, 115);
    lv_obj_set_pos(label, 205, 177);
    lv_obj_set_width(label, 98);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
#endif

    chip(parent, EQ_CHIP_X0, CP_TR("Gain"),
         model->mode == CRAZYPOD_EQ_STUDIO_GAIN, primary_color);
    chip(parent, EQ_CHIP_X0 + EQ_CHIP_STEP, CP_TR("Freq"),
         model->mode == CRAZYPOD_EQ_STUDIO_CUTOFF, primary_color);
    chip(parent, EQ_CHIP_X0 + 2 * EQ_CHIP_STEP, CP_TR("Q"),
         model->mode == CRAZYPOD_EQ_STUDIO_Q, primary_color);
    chip(parent, EQ_CHIP_X0 + 3 * EQ_CHIP_STEP, CP_TR("Precut"),
         model->mode == CRAZYPOD_EQ_STUDIO_PRECUT, primary_color);

#if EQ_SHOW_KEY_HINTS
    crazypod_ui_widget_box(
        parent, 0, 218, LCD_WIDTH, 22, 0, 0x050508, 245);
    label = crazypod_ui_widget_label(
        parent, CP_TR("Menu Done"), &lv_font_montserrat_8, WHITE, 125);
    lv_obj_set_pos(label, 14, 225);
    label = crazypod_ui_widget_label(
        parent, CP_TR("Select Edit"), &lv_font_montserrat_8, WHITE, 165);
    lv_obj_set_pos(label, 113, 225);
    label = crazypod_ui_widget_label(
        parent, model->editing ? CP_TR("Play Mode") : CP_TR("Play A/B"),
        &lv_font_montserrat_8, WHITE, 125);
    lv_obj_set_width(label, 82);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, 224, 225);
#endif
}

#endif
