#include "settings.h"

#include "../presentation/crazypod_ui_text.h"
#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include <stdbool.h>
#include <stdio.h>

#include "audio.h"
#include "powermgmt.h"
#include "timefuncs.h"

#include "../../crazypod_runtime_font.h"
#include "../../crazypod_screen_recording.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_status_bar.h"
#include "../../crazypod_color.h"

#define STATUS_WHITE 0xFFFFFF
#define STATUS_DARK 0x08080D
#define STATUS_RECORDING 0xFF3B30

/*
 * The bar holds a clock at one end and a battery at the other, and on the
 * compact panel that is all there is room for: twelve pixels tall, and the
 * battery is drawn at about half size so the clock keeps a readable face.
 */
#ifdef HAVE_CRAZYPOD_COMPACT_UI
#define STATUS_TIME_FONT (crazypod_runtime_font_at_size(11))
#define STATUS_TIME_X 3
#define STATUS_TIME_Y 1
#define STATUS_PLAYING_FONT (&lv_font_montserrat_8)
#define STATUS_PLAYING_X (LCD_WIDTH - 36)
#define STATUS_PLAYING_Y 2
#define STATUS_RECORDING_X (LCD_WIDTH - 46)
#define STATUS_RECORDING_Y 4
#define STATUS_RECORDING_SIZE 4
#define STATUS_BATTERY_X (LCD_WIDTH - 23)
#define STATUS_BATTERY_Y 2
#define STATUS_BATTERY_WIDTH 17
#define STATUS_BATTERY_HEIGHT 8
#define STATUS_BATTERY_RADIUS 2
#define STATUS_CHARGE_FONT (&lv_font_montserrat_8)
#define STATUS_BATTERY_CAP_X (LCD_WIDTH - 5)
#define STATUS_BATTERY_CAP_Y 4
#define STATUS_BATTERY_CAP_WIDTH 2
#define STATUS_BATTERY_CAP_HEIGHT 4
#else
#define STATUS_TIME_FONT (&lv_font_montserrat_12)
#define STATUS_TIME_X 34
#define STATUS_TIME_Y 7
#define STATUS_PLAYING_FONT (&lv_font_montserrat_10)
#define STATUS_PLAYING_X 241
#define STATUS_PLAYING_Y 11
#define STATUS_RECORDING_X 228
#define STATUS_RECORDING_Y 14
#define STATUS_RECORDING_SIZE 7
#define STATUS_BATTERY_X 258
#define STATUS_BATTERY_Y 11
#define STATUS_BATTERY_WIDTH 27
#define STATUS_BATTERY_HEIGHT 12
#define STATUS_BATTERY_RADIUS 3
#define STATUS_CHARGE_FONT (&lv_font_montserrat_8)
#define STATUS_BATTERY_CAP_X 287
#define STATUS_BATTERY_CAP_Y 15
#define STATUS_BATTERY_CAP_WIDTH 2
#define STATUS_BATTERY_CAP_HEIGHT 5
#endif

/* The fill sits one pixel inside the shell on every side. */
#define STATUS_BATTERY_FILL_MAX (STATUS_BATTERY_WIDTH - 2)

struct status_bar {
    lv_obj_t *time;
    lv_obj_t *battery;
    lv_obj_t *battery_fill;
    lv_obj_t *battery_cap;
    lv_obj_t *charge;
    lv_obj_t *playing;
    int rendered_minute;
    int rendered_battery_width;
    int rendered_charging;
    int rendered_playing;
    bool visible;
};

static struct status_bar status_bars[CRAZYPOD_STATUS_BAR_COUNT];
static lv_obj_t *recording_indicator;
static int rendered_recording = -1;

static struct status_bar *status_bar_at(int index)
{
    if(index < 0 || index >= CRAZYPOD_STATUS_BAR_COUNT)
        return NULL;
    return &status_bars[index];
}

void crazypod_status_bar_create(int index, lv_obj_t *screen)
{
    struct status_bar *bar = status_bar_at(index);

    if(bar == NULL || screen == NULL)
        return;
    bar->time = crazypod_ui_widget_label(
        screen, "00:00", STATUS_TIME_FONT,
        STATUS_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(bar->time, STATUS_TIME_X, STATUS_TIME_Y);

    bar->playing = crazypod_ui_widget_label(
        screen, LV_SYMBOL_PLAY, STATUS_PLAYING_FONT,
        STATUS_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(bar->playing, STATUS_PLAYING_X, STATUS_PLAYING_Y);
    lv_obj_add_flag(bar->playing, LV_OBJ_FLAG_HIDDEN);

    if(recording_indicator == NULL) {
        recording_indicator = crazypod_ui_widget_box(
            lv_layer_top(), STATUS_RECORDING_X, STATUS_RECORDING_Y,
            STATUS_RECORDING_SIZE, STATUS_RECORDING_SIZE,
            LV_RADIUS_CIRCLE, STATUS_RECORDING, LV_OPA_COVER);
        lv_obj_add_flag(
            recording_indicator, LV_OBJ_FLAG_HIDDEN);
    }

    bar->battery = crazypod_ui_widget_box(
        screen, STATUS_BATTERY_X, STATUS_BATTERY_Y,
        STATUS_BATTERY_WIDTH, STATUS_BATTERY_HEIGHT,
        STATUS_BATTERY_RADIUS, STATUS_WHITE, 64);
    bar->battery_fill = crazypod_ui_widget_box(
        bar->battery, 1, 1, STATUS_BATTERY_FILL_MAX,
        STATUS_BATTERY_HEIGHT - 2, STATUS_BATTERY_RADIUS - 1,
        STATUS_WHITE, LV_OPA_COVER);
    bar->charge = crazypod_ui_widget_label(
        bar->battery, LV_SYMBOL_CHARGE, STATUS_CHARGE_FONT,
        STATUS_DARK, LV_OPA_COVER);
    lv_obj_center(bar->charge);
    bar->battery_cap = crazypod_ui_widget_box(
        screen, STATUS_BATTERY_CAP_X, STATUS_BATTERY_CAP_Y,
        STATUS_BATTERY_CAP_WIDTH, STATUS_BATTERY_CAP_HEIGHT,
        1, STATUS_WHITE, 128);
    bar->rendered_minute = -1;
    bar->rendered_battery_width = -1;
    bar->rendered_charging = -1;
    bar->rendered_playing = -1;
    bar->visible = true;
}

void crazypod_status_bars_update(void)
{
    /* Nine bytes for "12:05 AM", not the five a 24-hour clock needs. */
    char time_text[16];
    struct tm *now = get_time();
    int minute = now->tm_hour * 60 + now->tm_min;
    int level = battery_level();
    int battery_width;
    bool charging = false;
    int status = audio_status();
    bool playing = (status & AUDIO_STATUS_PLAY) != 0 &&
                   (status & AUDIO_STATUS_PAUSE) == 0;
    bool screen_recording = crazypod_screen_recording_active();
    bool time_formatted = false;
    int i;

    if(recording_indicator != NULL &&
       rendered_recording != screen_recording) {
        if(screen_recording)
            lv_obj_remove_flag(
                recording_indicator, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(
                recording_indicator, LV_OBJ_FLAG_HIDDEN);
        rendered_recording = screen_recording;
    }

    if(level < 0)
        level = 0;
    if(level > 100)
        level = 100;
    /* A near-flat battery still shows a sliver, so "almost empty" and
     * "reporting nothing" do not look the same. */
    battery_width = level > 0
        ? (STATUS_BATTERY_FILL_MAX - 21 * STATUS_BATTERY_FILL_MAX / 24) +
          (21 * STATUS_BATTERY_FILL_MAX / 24) * level / 100
        : 0;
#if CONFIG_CHARGING >= CHARGING_MONITOR
    charging = charge_state > DISCHARGING;
#endif

    for(i = 0; i < CRAZYPOD_STATUS_BAR_COUNT; ++i) {
        struct status_bar *bar = &status_bars[i];

        if(bar->time == NULL)
            continue;
        if(!bar->visible)
            continue;
        if(bar->rendered_minute != minute) {
            if(!time_formatted) {
                crazypod_ui_text_clock(
                    time_text, sizeof(time_text), now->tm_hour,
                    now->tm_min, 0, false,
                    global_settings.timeformat != 0);
                time_formatted = true;
            }
            CP_LV_LABEL_SET_TEXT(bar->time, time_text);
            bar->rendered_minute = minute;
        }
        if(bar->rendered_battery_width != battery_width) {
            lv_obj_set_width(bar->battery_fill, battery_width);
            bar->rendered_battery_width = battery_width;
        }
        if(bar->rendered_charging != charging) {
            if(charging)
                lv_obj_remove_flag(
                    bar->charge, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(
                    bar->charge, LV_OBJ_FLAG_HIDDEN);
            bar->rendered_charging = charging;
        }
        if(bar->rendered_playing != playing) {
            if(playing)
                lv_obj_remove_flag(
                    bar->playing, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(
                    bar->playing, LV_OBJ_FLAG_HIDDEN);
            bar->rendered_playing = playing;
        }
    }
}

void crazypod_status_bar_set_palette(
    int index, uint32_t foreground, uint32_t background)
{
    struct status_bar *bar = status_bar_at(index);

    if(bar == NULL || bar->time == NULL)
        return;
    lv_obj_set_style_text_color(
        bar->time, crazypod_ui_color(foreground), 0);
    lv_obj_set_style_text_color(
        bar->playing, crazypod_ui_color(foreground), 0);
    lv_obj_set_style_bg_color(
        bar->battery, crazypod_ui_color(foreground), 0);
    lv_obj_set_style_bg_color(
        bar->battery_fill, crazypod_ui_color(foreground), 0);
    lv_obj_set_style_bg_color(
        bar->battery_cap, crazypod_ui_color(foreground), 0);
    lv_obj_set_style_text_color(
        bar->charge, crazypod_ui_color(background), 0);
}

void crazypod_status_bar_foreground(int index)
{
    struct status_bar *bar = status_bar_at(index);

    if(bar == NULL || bar->time == NULL)
        return;
    lv_obj_move_foreground(bar->time);
    lv_obj_move_foreground(bar->playing);
    if(recording_indicator != NULL)
        lv_obj_move_foreground(recording_indicator);
}

void crazypod_status_bar_set_visible(int index, bool visible)
{
    struct status_bar *bar = status_bar_at(index);
    lv_obj_t *objects[4];
    int i;

    if(bar == NULL || bar->time == NULL)
        return;
    if(bar->visible == visible)
        return;
    bar->visible = visible;
    objects[0] = bar->time;
    objects[1] = bar->battery;
    objects[2] = bar->battery_cap;
    objects[3] = NULL;
    for(i = 0; objects[i] != NULL; ++i) {
        if(visible)
            lv_obj_remove_flag(objects[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(objects[i], LV_OBJ_FLAG_HIDDEN);
    }
    if(!visible) {
        lv_obj_add_flag(bar->charge, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bar->playing, LV_OBJ_FLAG_HIDDEN);
    }
    if(visible) {
        bar->rendered_charging = -1;
        bar->rendered_playing = -1;
    }
}

#endif
