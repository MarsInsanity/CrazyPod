#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "settings.h"

#include "../../../crazypod_runtime_font.h"
#include "../../presentation/crazypod_ui_metrics.h"
#include "../../presentation/crazypod_ui_text.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_clock_screen.h"
#include "../../../crazypod_color.h"

#define COLOR_WHITE 0xFFFFFF

/*
 * The watch faces are the one part of the product already drawn as ink on
 * paper: a white dial with black hands, on a near-white card. Everything
 * else is a dark UI, which is what crazypod_ui_color() inverts for the
 * monochrome panel -- and applying that here turns the white face black,
 * which is what the Mini showed. So the faces name the panel's own shades
 * and are drawn with the helpers that pass them through unmapped.
 */
#ifdef HAVE_CRAZYPOD_MONO_UI
#define FACE_BOX crazypod_ui_widget_box_shade
#define FACE_LABEL crazypod_ui_widget_label_shade
#define FACE_INK crazypod_ui_shade
#define FACE_PAPER 0xFFFFFF
#define FACE_DARK 0x000000
#define FACE_MUTED 0x555555
#define FACE_FAINT 0xAAAAAA
#else
#define FACE_BOX crazypod_ui_widget_box
#define FACE_LABEL crazypod_ui_widget_label
#define FACE_INK crazypod_ui_color
#define FACE_PAPER 0xF9F9F7
#define FACE_DARK 0x0E0E0E
#define FACE_MUTED 0x5C5C5C
#define FACE_FAINT 0x949494
#endif
/* The eight minor tick marks, a shade down from the ink. */
#define MARK_FAINT FACE_FAINT

/*
 * The dial is rebuilt on a timer -- four times a second for the clock,
 * ten for the stopwatch -- and it is twenty-five objects of which sixteen
 * carry a transform, so every one of them renders through its own layer.
 * Only the three hands and a couple of numbers actually change, so build
 * the face once and rotate the hands in place: the twelve tick marks then
 * never redraw at all, because nothing invalidates them.
 *
 * Cleared by crazypod_clock_screen_forget() before the pane is cleaned.
 */
static struct {
    lv_obj_t *panel;
    lv_obj_t *hour_hand;
    lv_obj_t *minute_hand;
    lv_obj_t *second_hand;
    lv_obj_t *digital;
    lv_obj_t *date;
    lv_obj_t *running;
    /* What each of those is currently showing. A rotation or a label set
     * invalidates whether or not the value moved, and at ten ticks a
     * second most of them have not: the hour hand moves twice a minute
     * and the date once a day. */
    int hour_angle;
    int minute_angle;
    int second_angle;
    int shown_time;
    int shown_date;
    bool shown_running;
    int lap_count;
    bool stopwatch;
    int style;
} face;

void crazypod_clock_screen_forget(void)
{
    memset(&face, 0, sizeof(face));
}

static bool face_usable(bool stopwatch, int style)
{
    return face.panel != NULL && lv_obj_is_valid(face.panel) &&
           face.stopwatch == stopwatch && face.style == style &&
           face.hour_hand != NULL && face.minute_hand != NULL &&
           face.second_hand != NULL && face.digital != NULL;
}

bool crazypod_clock_screen_dial_ready(void)
{
    return face_usable(false, 0);
}

static void set_hand(lv_obj_t *hand, int *shown, int angle_tenths)
{
    if(hand == NULL || *shown == angle_tenths)
        return;
    *shown = angle_tenths;
    lv_obj_set_style_transform_rotation(hand, angle_tenths, 0);
}

static lv_obj_t *make_clock_hand(
    lv_obj_t *dial, int *shown, int center, int length, int width,
    int angle_tenths, uint32_t color)
{
    lv_obj_t *hand = FACE_BOX(
        dial, center - width / 2, center - length,
        width, length, width, color, LV_OPA_COVER);

    lv_obj_set_style_transform_pivot_x(hand, width / 2, 0);
    lv_obj_set_style_transform_pivot_y(hand, length, 0);
    lv_obj_set_style_transform_rotation(hand, angle_tenths, 0);
    *shown = angle_tenths;
    return hand;
}

static lv_obj_t *make_analog_clock(
    lv_obj_t *parent, int x, int y, int size,
    int hour, int minute, int second_tenths,
    uint32_t dial_color, uint32_t ink_color)
{
    lv_obj_t *dial = FACE_BOX(
        parent, x, y, size, size, LV_RADIUS_CIRCLE,
        dial_color, LV_OPA_COVER);
    int center = size / 2;
    /* The marks scale with the dial: ten pixels of tick on a 140px face is
     * a seventh of its radius, and the same seventh of a 56px one is four.
     * Fixed sizes here filled the small dial in solid. */
    int inset = size >= 100 ? 7 : 3;
    int long_tick = size >= 100 ? 10 : 5;
    int short_tick = size >= 100 ? 6 : 3;
    int tick;

    lv_obj_set_style_border_width(dial, size >= 100 ? 2 : 1, 0);
    lv_obj_set_style_border_color(dial, FACE_INK(ink_color), 0);
    lv_obj_set_style_border_opa(dial, 220, 0);
    for(tick = 0; tick < 12; ++tick) {
        int width = tick % 3 == 0 ? 2 : 1;
        int height = tick % 3 == 0 ? long_tick : short_tick;
        lv_obj_t *mark = FACE_BOX(
            dial, center - width / 2, inset,
            width, height, width,
            tick % 3 == 0 ? ink_color : MARK_FAINT,
            tick % 3 == 0 ? 235 : 180);
        lv_obj_set_style_transform_pivot_x(mark, width / 2, 0);
        lv_obj_set_style_transform_pivot_y(mark, center - inset, 0);
        lv_obj_set_style_transform_rotation(mark, tick * 300, 0);
    }
    face.hour_hand = make_clock_hand(
        dial, &face.hour_angle, center, size * 25 / 100, 4,
        ((hour % 12) * 30 + minute / 2) * 10, ink_color);
    face.minute_hand = make_clock_hand(
        dial, &face.minute_angle, center, size * 36 / 100, 3,
        minute * 60 + second_tenths / 10, ink_color);
    face.second_hand = make_clock_hand(
        dial, &face.second_angle, center, size * 42 / 100, 1,
        second_tenths * 6, ink_color);
    {
        int hub = size >= 100 ? 8 : 4;

        FACE_BOX(
            dial, center - hub / 2, center - hub / 2, hub, hub,
            LV_RADIUS_CIRCLE, ink_color, LV_OPA_COVER);
    }
    return dial;
}

void crazypod_clock_screen_render(
    lv_obj_t *content,
    const struct crazypod_clock_screen_time *time)
{
    static const char *const weekdays[] = {
        CP_TR("Sunday"), CP_TR("Monday"), CP_TR("Tuesday"), CP_TR("Wednesday"),
        CP_TR("Thursday"), CP_TR("Friday"), CP_TR("Saturday")
    };
    static const char *const months[] = {
        CP_TR("January"), CP_TR("February"), CP_TR("March"), CP_TR("April"), CP_TR("May"), CP_TR("June"),
        CP_TR("July"), CP_TR("August"), CP_TR("September"), CP_TR("October"), CP_TR("November"), CP_TR("December")
    };
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[64];

    if(crazypod_clock_screen_refresh(time))
        return;
    memset(&face, 0, sizeof(face));

#ifdef HAVE_CRAZYPOD_COMPACT_UI
    /*
     * One column, not two: the dial centred with the time and the date
     * stacked under it. The wide card the large canvas draws would be
     * twice this panel across.
     */
    /* From under the status bar down. Painting the strip as well would
     * cover the clock and battery, which are drawn before the route's
     * content and not by it. */
    FACE_BOX(
        content, 0, CRAZYPOD_METRIC_STATUS_HEIGHT, LCD_WIDTH,
        LCD_HEIGHT - CRAZYPOD_METRIC_STATUS_HEIGHT, 0,
        FACE_PAPER, LV_OPA_COVER);
    panel = FACE_BOX(
        content, CRAZYPOD_METRIC_FACE_PANEL_X,
        CRAZYPOD_METRIC_FACE_PANEL_Y,
        CRAZYPOD_METRIC_FACE_PANEL_WIDTH,
        CRAZYPOD_METRIC_FACE_PANEL_HEIGHT,
        CRAZYPOD_METRIC_FACE_PANEL_RADIUS, FACE_PAPER, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, FACE_INK(FACE_DARK), 0);
    lv_obj_set_style_border_opa(panel, 90, 0);
    make_analog_clock(
        panel, CRAZYPOD_METRIC_FACE_DIAL_X, CRAZYPOD_METRIC_FACE_DIAL_Y,
        CRAZYPOD_METRIC_FACE_DIAL_SIZE, time->hour, time->minute,
        time->second_tenths, FACE_PAPER, FACE_DARK);

    crazypod_ui_text_clock(text, sizeof(text), time->hour, time->minute,
                           time->second, true,
                           global_settings.timeformat != 0);
    label = FACE_LABEL(
        panel, text,
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_FACE_TIME_SIZE),
        FACE_DARK, LV_OPA_COVER);
    lv_obj_set_width(label, CRAZYPOD_METRIC_FACE_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_FACE_TEXT_X,
                   CRAZYPOD_METRIC_FACE_TIME_Y);
    face.digital = label;
    snprintf(text, sizeof(text), "%s, %s %d",
             weekdays[time->weekday], months[time->month],
             time->month_day);
    label = FACE_LABEL(
        panel, text,
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_FACE_DETAIL_SIZE),
        FACE_MUTED, LV_OPA_COVER);
    lv_obj_set_width(label, CRAZYPOD_METRIC_FACE_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_FACE_TEXT_X,
                   CRAZYPOD_METRIC_FACE_DETAIL_Y);
    face.date = label;
#else
    FACE_BOX(
        content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
        FACE_PAPER, LV_OPA_COVER);
    panel = FACE_BOX(
        content, 10, 40, 300, 188, 12, 0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, FACE_INK(0x000000), 0);
    lv_obj_set_style_border_opa(panel, 34, 0);
    make_analog_clock(
        panel, 14, 23, 140, time->hour, time->minute,
        time->second_tenths, 0xFFFFFF, FACE_DARK);

    label = FACE_LABEL(
        panel, CP_TR("LOCAL TIME"), &lv_font_montserrat_8,
        FACE_MUTED, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 170, 34);
    crazypod_ui_text_clock(text, sizeof(text), time->hour, time->minute,
                           time->second, true,
                           global_settings.timeformat != 0);
    label = FACE_LABEL(
        panel, text, &lv_font_montserrat_24,
        FACE_DARK, LV_OPA_COVER);
    lv_obj_set_pos(label, 170, 53);
    face.digital = label;
    FACE_BOX(
        panel, 170, 86, 112, 1, 0, FACE_DARK, 210);
    snprintf(text, sizeof(text), "%s\n%s %d",
             weekdays[time->weekday], months[time->month],
             time->month_day);
    label = FACE_LABEL(
        panel, text, &lv_font_montserrat_10,
        FACE_MUTED, LV_OPA_COVER);
    lv_obj_set_pos(label, 170, 97);
    face.date = label;
    label = FACE_LABEL(
        panel, CP_TR("DEVICE TIME"), &lv_font_montserrat_8, FACE_FAINT, 230);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_set_pos(label, 170, 132);
#endif

    face.panel = panel;
    face.stopwatch = false;
    face.style = 0;
    face.shown_time = (time->hour * 60 + time->minute) * 60 + time->second;
    face.shown_date = time->month_day;
    lv_obj_null_on_delete(&face.panel);
    lv_obj_null_on_delete(&face.hour_hand);
    lv_obj_null_on_delete(&face.minute_hand);
    lv_obj_null_on_delete(&face.second_hand);
    lv_obj_null_on_delete(&face.digital);
    lv_obj_null_on_delete(&face.date);
}

bool crazypod_clock_screen_refresh(
    const struct crazypod_clock_screen_time *time)
{
    static const char *const weekdays[] = {
        CP_TR("Sunday"), CP_TR("Monday"), CP_TR("Tuesday"), CP_TR("Wednesday"),
        CP_TR("Thursday"), CP_TR("Friday"), CP_TR("Saturday")
    };
    static const char *const months[] = {
        CP_TR("January"), CP_TR("February"), CP_TR("March"), CP_TR("April"), CP_TR("May"), CP_TR("June"),
        CP_TR("July"), CP_TR("August"), CP_TR("September"), CP_TR("October"), CP_TR("November"), CP_TR("December")
    };
    char text[64];
    int clock_time;

    if(!face_usable(false, 0))
        return false;
    set_hand(face.hour_hand, &face.hour_angle,
             ((time->hour % 12) * 30 + time->minute / 2) * 10);
    set_hand(face.minute_hand, &face.minute_angle,
             time->minute * 60 + time->second_tenths / 10);
    set_hand(face.second_hand, &face.second_angle,
             time->second_tenths * 6);
    clock_time = (time->hour * 60 + time->minute) * 60 + time->second;
    if(clock_time != face.shown_time) {
        face.shown_time = clock_time;
        crazypod_ui_text_clock(text, sizeof(text), time->hour,
                               time->minute, time->second, true,
                               global_settings.timeformat != 0);
        CP_LV_LABEL_SET_TEXT(face.digital, text);
    }
    if(time->month_day != face.shown_date) {
        face.shown_date = time->month_day;
        snprintf(text, sizeof(text), "%s\n%s %d",
                 weekdays[time->weekday], months[time->month],
                 time->month_day);
        CP_LV_LABEL_SET_TEXT(face.date, text);
    }
    return true;
}

void crazypod_stopwatch_screen_render(
    lv_obj_t *content,
    const struct crazypod_stopwatch_screen_model *model)
{
    static const char *const style_names[] = {
        CP_TR("CLASSIC SILVER"), CP_TR("OBSIDIAN GOLD"), CP_TR("CHAMPAGNE GOLD")
    };
    static const uint32_t canvas_colors[] = {
        0xF9F9F7, 0xF2F2F2, 0xFFFFFF
    };
    static const uint32_t dial_colors[] = {
        0xFFFFFF, 0xFFFFFF, 0xEDEDED
    };
    static const uint32_t ink_colors[] = {
        0x0E0E0E, 0x2C2416, 0x3B2A10
    };
    unsigned total_hundredths =
        (unsigned)(model->elapsed_ticks * 100 /
                   model->ticks_per_second);
    unsigned minutes = total_hundredths / 6000;
    unsigned seconds = total_hundredths / 100 % 60;
    unsigned hundredths = total_hundredths % 100;
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[32];
    int first_lap;
    int lap;
    int style = model->style % 3;

    if(crazypod_stopwatch_screen_refresh(model))
        return;
    memset(&face, 0, sizeof(face));

#ifdef HAVE_CRAZYPOD_COMPACT_UI
    /*
     * The dial, the running time under it and the state under that. The
     * lap table, the style caption and the key legend the large canvas
     * prints beside the dial have nowhere to go on a 138px panel, and the
     * laps are on their own route anyway.
     */
    (void)first_lap;
    (void)lap;
    FACE_BOX(
        content, 0, CRAZYPOD_METRIC_STATUS_HEIGHT, LCD_WIDTH,
        LCD_HEIGHT - CRAZYPOD_METRIC_STATUS_HEIGHT, 0,
        FACE_PAPER, LV_OPA_COVER);
    panel = FACE_BOX(
        content, CRAZYPOD_METRIC_FACE_PANEL_X,
        CRAZYPOD_METRIC_FACE_PANEL_Y,
        CRAZYPOD_METRIC_FACE_PANEL_WIDTH,
        CRAZYPOD_METRIC_FACE_PANEL_HEIGHT,
        CRAZYPOD_METRIC_FACE_PANEL_RADIUS, FACE_PAPER, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, FACE_INK(FACE_DARK), 0);
    lv_obj_set_style_border_opa(panel, 90, 0);
    make_analog_clock(
        panel, CRAZYPOD_METRIC_FACE_DIAL_X, CRAZYPOD_METRIC_FACE_DIAL_Y,
        CRAZYPOD_METRIC_FACE_DIAL_SIZE, (int)(minutes / 60),
        (int)minutes % 60,
        (int)seconds * 10 + (int)hundredths / 10,
        FACE_PAPER, FACE_DARK);
    snprintf(text, sizeof(text), "%02u:%02u.%02u",
             minutes, seconds, hundredths);
    label = FACE_LABEL(
        panel, text,
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_FACE_TIME_SIZE),
        FACE_DARK, LV_OPA_COVER);
    lv_obj_set_width(label, CRAZYPOD_METRIC_FACE_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_FACE_TEXT_X,
                   CRAZYPOD_METRIC_FACE_TIME_Y);
    face.digital = label;
    if(model->lap_count > 0)
        snprintf(text, sizeof(text), CP_FMT("%s  ·  %d LAPS"),
                 model->running ? CP_TR("RUNNING") : CP_TR("PAUSED"),
                 model->lap_count);
    else
        snprintf(text, sizeof(text), "%s",
                 model->running ? CP_TR("RUNNING") : CP_TR("PAUSED"));
    label = FACE_LABEL(
        panel, text,
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_FACE_DETAIL_SIZE),
        FACE_MUTED, LV_OPA_COVER);
    lv_obj_set_width(label, CRAZYPOD_METRIC_FACE_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_FACE_TEXT_X,
                   CRAZYPOD_METRIC_FACE_DETAIL_Y);
    face.running = label;
#else
    FACE_BOX(
        content, 0, 32, LCD_WIDTH, LCD_HEIGHT - 32, 0,
        canvas_colors[style], LV_OPA_COVER);
    panel = FACE_BOX(
        content, 10, 40, 300, 188, 12, 0xFFFFFF, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, FACE_INK(0x000000), 0);
    lv_obj_set_style_border_opa(panel, 34, 0);
    make_analog_clock(
        panel, 8, 23, 140, (int)(minutes / 60),
        (int)minutes % 60,
        (int)seconds * 10 + (int)hundredths / 10,
        dial_colors[style], ink_colors[style]);
    label = crazypod_ui_widget_label(
        panel, style_names[style], &lv_font_montserrat_8,
        ink_colors[style], 135);
    lv_obj_set_width(label, 140);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 8, 168);

    label = crazypod_ui_widget_label(
        panel, CP_TR("CHRONOGRAPH"), &lv_font_montserrat_8,
        ink_colors[style], 110);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 166, 22);
    snprintf(text, sizeof(text), "%02u:%02u.%02u",
             minutes, seconds, hundredths);
    label = crazypod_ui_widget_label(
        panel, text, &lv_font_montserrat_24,
        ink_colors[style], LV_OPA_COVER);
    lv_obj_set_pos(label, 166, 39);
    face.digital = label;
    label = crazypod_ui_widget_label(
        panel, model->running ? CP_TR("RUNNING") : CP_TR("PAUSED"),
        &lv_font_montserrat_8, ink_colors[style], 225);
    lv_obj_set_pos(label, 166, 69);
    face.running = label;
    if(model->lap_count > 0) {
        snprintf(text, sizeof(text), CP_FMT("%d LAPS"), model->lap_count);
        label = crazypod_ui_widget_label(
            panel, text, &lv_font_montserrat_8,
            ink_colors[style], 140);
        lv_obj_set_pos(label, 224, 69);
    }
    crazypod_ui_widget_box(
        panel, 166, 84, 122, 1, 0, ink_colors[style], 52);
    first_lap = model->lap_count > 4 ? model->lap_count - 4 : 0;
    if(model->lap_count > 0) {
        label = crazypod_ui_widget_label(
            panel, CP_TR("LAP       TOTAL"), &lv_font_montserrat_8,
            ink_colors[style], 105);
        lv_obj_set_pos(label, 168, 90);
    }
    for(lap = first_lap; lap < model->lap_count; ++lap) {
        unsigned lap_hundredths =
            (unsigned)(model->laps[lap] * 100 /
                       model->ticks_per_second);
        snprintf(text, sizeof(text), CP_FMT("%02d     %02u:%02u.%02u"),
                 lap + 1, lap_hundredths / 6000,
                 lap_hundredths / 100 % 60,
                 lap_hundredths % 100);
        label = crazypod_ui_widget_label(
            panel, text, &lv_font_montserrat_8,
            ink_colors[style], 225);
        lv_obj_set_pos(label, 168, 104 + (lap - first_lap) * 15);
    }
    if(model->lap_count == 0) {
        label = crazypod_ui_widget_label(
            panel,
            CP_TR("CENTER  START / PAUSE\nRIGHT   RECORD LAP\nLEFT    RESET"),
            &lv_font_montserrat_8, ink_colors[style], 150);
        lv_obj_set_pos(label, 166, 101);
    }
    label = crazypod_ui_widget_label(
        panel,
        model->reset_armed
            ? CP_TR("Press LEFT again to reset")
            : CP_TR("Wheel changes style before first lap"),
        &lv_font_montserrat_8, 0x949494, 210);
    lv_obj_set_width(label, 136);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(label, 166, 168);
#endif

    face.panel = panel;
    face.stopwatch = true;
    face.style = style;
    face.lap_count = model->lap_count;
    face.shown_time = (int)total_hundredths;
    face.shown_running = model->running;
    lv_obj_null_on_delete(&face.panel);
    lv_obj_null_on_delete(&face.hour_hand);
    lv_obj_null_on_delete(&face.minute_hand);
    lv_obj_null_on_delete(&face.second_hand);
    lv_obj_null_on_delete(&face.digital);
    lv_obj_null_on_delete(&face.running);
}

bool crazypod_stopwatch_screen_refresh(
    const struct crazypod_stopwatch_screen_model *model)
{
    unsigned total_hundredths;
    unsigned minutes;
    unsigned seconds;
    unsigned hundredths;
    char text[32];

    /* A lap arriving changes the list under the dial, and the style can
     * only change before the first lap; both mean a rebuild. Ticking
     * does not. */
    if(!face_usable(true, model->style % 3) ||
       face.lap_count != model->lap_count || face.running == NULL)
        return false;

    total_hundredths = (unsigned)(model->elapsed_ticks * 100 /
                                  model->ticks_per_second);
    minutes = total_hundredths / 6000;
    seconds = total_hundredths / 100 % 60;
    hundredths = total_hundredths % 100;

    set_hand(face.hour_hand, &face.hour_angle,
             (((int)(minutes / 60) % 12) * 30 +
              ((int)minutes % 60) / 2) * 10);
    set_hand(face.minute_hand, &face.minute_angle,
             ((int)minutes % 60) * 60 + (int)seconds / 6);
    set_hand(face.second_hand, &face.second_angle,
             ((int)seconds * 10 + (int)hundredths / 10) * 6);
    if((int)total_hundredths != face.shown_time) {
        face.shown_time = (int)total_hundredths;
        snprintf(text, sizeof(text), "%02u:%02u.%02u",
                 minutes, seconds, hundredths);
        CP_LV_LABEL_SET_TEXT(face.digital, text);
    }
    if(model->running != face.shown_running) {
        face.shown_running = model->running;
        CP_LV_LABEL_SET_TEXT(
            face.running,
            model->running ? CP_TR("RUNNING") : CP_TR("PAUSED"));
    }
    return true;
}

#endif
