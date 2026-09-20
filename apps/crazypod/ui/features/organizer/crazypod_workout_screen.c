#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include <stdio.h>
#include <string.h>

#include "../../../crazypod_workouts.h"
#include "../../../crazypod_runtime_font.h"
#include "../../presentation/crazypod_ui_metrics.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_workout_screen.h"
#include "../../../crazypod_color.h"

#define CRAZYPOD_WORKOUT_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_WORKOUT_WHITE 0xFFFFFF
#define CRAZYPOD_WORKOUT_RUNNING 0xA8F12D
#define CRAZYPOD_WORKOUT_PAUSED 0xFFB340

/* The large canvas sets its readout beside the ring and ragged left; the
 * small one stacks it under the ring, where it has to be centred. */
#if CRAZYPOD_METRIC_WORKOUT_SHOW_LEGEND
#define CRAZYPOD_WORKOUT_ALIGN LV_TEXT_ALIGN_LEFT
#define CRAZYPOD_WORKOUT_RING_FONT (&lv_font_montserrat_24)
#else
#define CRAZYPOD_WORKOUT_ALIGN LV_TEXT_ALIGN_CENTER
#define CRAZYPOD_WORKOUT_RING_FONT (&lv_font_montserrat_16)
#endif

static void format_duration(
    char *text, size_t size, uint32_t seconds)
{
    snprintf(text, size, "%02lu:%02lu:%02lu",
             (unsigned long)(seconds / 3600u),
             (unsigned long)(seconds / 60u % 60u),
             (unsigned long)(seconds % 60u));
}


/*
 * The active workout is a stopwatch that logs what it timed, and it was
 * rebuilt ten times a second for the same reason the stopwatch was: a
 * tick asked for a route render, and a route render cleans the pane and
 * builds it again. Only the elapsed time moves, and the ring and the
 * status line only when the timer is paused or resumed.
 *
 * Cleared by crazypod_workout_screen_forget() before the pane is cleaned.
 */
static struct {
    lv_obj_t *backdrop;
    lv_obj_t *ring;
    lv_obj_t *ring_icon;
    lv_obj_t *elapsed;
    lv_obj_t *status;
    uint32_t shown_seconds;
    int activity;
    bool running;
} face;

void crazypod_workout_screen_forget(void)
{
    memset(&face, 0, sizeof(face));
}

static bool face_usable(int activity)
{
    return face.backdrop != NULL && lv_obj_is_valid(face.backdrop) &&
           face.activity == activity && face.ring != NULL &&
           face.ring_icon != NULL && face.elapsed != NULL &&
           face.status != NULL;
}

static void set_running_look(bool running)
{
    uint32_t color = running
        ? CRAZYPOD_WORKOUT_RUNNING : CRAZYPOD_WORKOUT_PAUSED;

    lv_obj_set_style_border_color(face.ring, crazypod_ui_color(color), 0);
    CP_LV_LABEL_SET_TEXT(
        face.ring_icon, running ? LV_SYMBOL_PLAY : CP_TR("II"));
    lv_obj_set_style_text_color(face.ring_icon, crazypod_ui_color(color), 0);
    CP_LV_LABEL_SET_TEXT(
        face.status, running ? CP_TR("RUNNING") : CP_TR("PAUSED"));
    lv_obj_set_style_text_color(face.status, crazypod_ui_color(color), 0);
}

bool crazypod_workout_screen_refresh_active(
    int activity, bool running, uint32_t seconds)
{
    char elapsed[24];

    if(!face_usable(activity))
        return false;
    if(seconds != face.shown_seconds) {
        face.shown_seconds = seconds;
        format_duration(elapsed, sizeof(elapsed), seconds);
        CP_LV_LABEL_SET_TEXT(face.elapsed, elapsed);
    }
    if(running != face.running) {
        face.running = running;
        set_running_look(running);
    }
    return true;
}

void crazypod_workout_screen_render_ready(
    lv_obj_t *content, int activity)
{
    lv_obj_t *panel;
    lv_obj_t *label;

    crazypod_ui_widget_box(
        content, 0, CRAZYPOD_METRIC_STATUS_HEIGHT, LCD_WIDTH,
        LCD_HEIGHT - CRAZYPOD_METRIC_STATUS_HEIGHT, 0,
        0x050505, LV_OPA_COVER);
    panel = crazypod_ui_widget_box(
        content, CRAZYPOD_METRIC_WORKOUT_PANEL_X,
        CRAZYPOD_METRIC_WORKOUT_PANEL_Y,
        CRAZYPOD_METRIC_WORKOUT_PANEL_WIDTH,
        CRAZYPOD_METRIC_WORKOUT_PANEL_HEIGHT,
        CRAZYPOD_METRIC_WORKOUT_PANEL_RADIUS, 0x111512, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, crazypod_ui_color(0xA8F12D), 0);
    lv_obj_set_style_border_opa(panel, 100, 0);
    label = crazypod_ui_widget_label(
        panel, crazypod_workout_activity_title(activity),
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_WORKOUT_TITLE_SIZE),
        CRAZYPOD_WORKOUT_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, CRAZYPOD_METRIC_WORKOUT_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_WORKOUT_TEXT_X,
                   CRAZYPOD_METRIC_WORKOUT_TITLE_Y);
    label = crazypod_ui_widget_label(
        panel, CP_TR("READY"),
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_WORKOUT_STATE_SIZE),
        0xA8F12D, LV_OPA_COVER);
    lv_obj_set_width(label, CRAZYPOD_METRIC_WORKOUT_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_WORKOUT_TEXT_X,
                   CRAZYPOD_METRIC_WORKOUT_STATE_Y);
    label = crazypod_ui_widget_label(
        panel,
#if CRAZYPOD_METRIC_WORKOUT_SHOW_LEGEND
        CP_TR("TIME ONLY\nNo motion, distance, or calorie estimates"),
#else
        /* The caveat, without the sentence there is no room for. */
        CP_TR("TIME ONLY"),
#endif
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_WORKOUT_NOTE_SIZE),
        CRAZYPOD_WORKOUT_WHITE, 145);
    lv_obj_set_width(label, CRAZYPOD_METRIC_WORKOUT_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_WORKOUT_TEXT_X,
                   CRAZYPOD_METRIC_WORKOUT_NOTE_Y);
    label = crazypod_ui_widget_label(
        panel, CP_TR("CENTER  START"),
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_WORKOUT_NOTE_SIZE),
        0xA8F12D, 230);
    lv_obj_set_width(label, CRAZYPOD_METRIC_WORKOUT_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_WORKOUT_TEXT_X,
                   CRAZYPOD_METRIC_WORKOUT_HINT_Y);
}

void crazypod_workout_screen_render_active(
    lv_obj_t *content, int activity, bool running, uint32_t seconds)
{
    lv_obj_t *backdrop;
    lv_obj_t *ring;
    lv_obj_t *label;
    char elapsed[24];

    if(crazypod_workout_screen_refresh_active(activity, running, seconds))
        return;
    memset(&face, 0, sizeof(face));

    backdrop = crazypod_ui_widget_box(
        content, 0, CRAZYPOD_METRIC_STATUS_HEIGHT, LCD_WIDTH,
        LCD_HEIGHT - CRAZYPOD_METRIC_STATUS_HEIGHT, 0,
        0x050505, LV_OPA_COVER);
    ring = crazypod_ui_widget_box(
        content, CRAZYPOD_METRIC_WORKOUT_RING_X,
        CRAZYPOD_METRIC_WORKOUT_RING_Y,
        CRAZYPOD_METRIC_WORKOUT_RING_SIZE,
        CRAZYPOD_METRIC_WORKOUT_RING_SIZE, LV_RADIUS_CIRCLE,
        0x0A0A0A, LV_OPA_COVER);
    lv_obj_set_style_border_width(
        ring, CRAZYPOD_METRIC_WORKOUT_RING_BORDER, 0);
    lv_obj_set_style_border_opa(ring, 235, 0);
    /*
     * Montserrat, not the runtime face: LV_SYMBOL_PLAY is in LVGL's own
     * symbol range, which the AOT text fonts do not carry, so resolving
     * this through the size ladder left the ring empty.
     */
    label = crazypod_ui_widget_label(
        ring, LV_SYMBOL_PLAY, CRAZYPOD_WORKOUT_RING_FONT,
        CRAZYPOD_WORKOUT_RUNNING, LV_OPA_COVER);
    lv_obj_center(label);
    face.ring = ring;
    face.ring_icon = label;
    label = crazypod_ui_widget_label(
        content, crazypod_workout_activity_title(activity),
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_WORKOUT_NOTE_SIZE),
        CRAZYPOD_WORKOUT_WHITE, 165);
    lv_obj_set_width(label, CRAZYPOD_METRIC_WORKOUT_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, CRAZYPOD_WORKOUT_ALIGN, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_WORKOUT_TEXT_X,
                   CRAZYPOD_METRIC_WORKOUT_ACTIVITY_Y);
    format_duration(elapsed, sizeof(elapsed), seconds);
    label = crazypod_ui_widget_label(
        content, elapsed,
        crazypod_runtime_font_at_size(
            CRAZYPOD_METRIC_WORKOUT_ELAPSED_SIZE),
        CRAZYPOD_WORKOUT_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, CRAZYPOD_METRIC_WORKOUT_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, CRAZYPOD_WORKOUT_ALIGN, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_WORKOUT_TEXT_X,
                   CRAZYPOD_METRIC_WORKOUT_ELAPSED_Y);
    face.elapsed = label;
#if CRAZYPOD_METRIC_WORKOUT_SHOW_LEGEND
    crazypod_ui_widget_box(
        content, 174, 108, 126, 1, 0,
        CRAZYPOD_WORKOUT_WHITE, 90);
#endif
    label = crazypod_ui_widget_label(
        content, CP_TR("RUNNING"),
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_WORKOUT_NOTE_SIZE),
        CRAZYPOD_WORKOUT_RUNNING, 235);
    lv_obj_set_width(label, CRAZYPOD_METRIC_WORKOUT_TEXT_WIDTH);
    lv_obj_set_style_text_align(label, CRAZYPOD_WORKOUT_ALIGN, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_WORKOUT_TEXT_X,
                   CRAZYPOD_METRIC_WORKOUT_STATUS_Y);
    face.status = label;
#if CRAZYPOD_METRIC_WORKOUT_SHOW_LEGEND
    label = crazypod_ui_widget_label(
        content,
        CP_TR("CENTER  PAUSE / RESUME\nPLAY  FINISH\nTIME-ONLY LOG"),
        &lv_font_montserrat_8, CRAZYPOD_WORKOUT_WHITE, 125);
    lv_obj_set_pos(label, 174, 145);
#endif

    face.backdrop = backdrop;
    face.activity = activity;
    face.shown_seconds = seconds;
    /* Built as running, then corrected: one place decides the look. */
    face.running = true;
    if(!running) {
        face.running = false;
        set_running_look(false);
    }
    lv_obj_null_on_delete(&face.backdrop);
    lv_obj_null_on_delete(&face.ring);
    lv_obj_null_on_delete(&face.ring_icon);
    lv_obj_null_on_delete(&face.elapsed);
    lv_obj_null_on_delete(&face.status);
}

void crazypod_workout_screen_render_summary(lv_obj_t *content)
{
    lv_obj_t *panel;
    lv_obj_t *label;
    uint32_t total_seconds = 0;
    char text[160];
    int i;

    for(i = 0; i < crazypod_workouts_count(); ++i) {
        const struct crazypod_workout *workout =
            crazypod_workout_get(i);
        if(workout != NULL)
            total_seconds += workout->duration_seconds;
    }
    panel = crazypod_ui_widget_box(
        content, 18, 52, 284, 150, 14, 0x111512, 238);
    snprintf(text, sizeof(text),
             CP_FMT("WORKOUT SUMMARY\n\n%d saved workouts\n"
                    "%lu total minutes\n\n"
                    "Metrics: elapsed time only\n"
                    "No sensor data is fabricated."),
             crazypod_workouts_count(),
             (unsigned long)(total_seconds / 60u));
    label = crazypod_ui_widget_label(
        panel, text, CRAZYPOD_WORKOUT_FONT,
        CRAZYPOD_WORKOUT_WHITE, 230);
    lv_obj_set_pos(label, 16, 14);
    lv_obj_set_width(label, 252);
}

void crazypod_workout_screen_render_detail(
    lv_obj_t *content, int workout_index)
{
    const struct crazypod_workout *workout =
        crazypod_workout_get(workout_index);
    lv_obj_t *panel;
    lv_obj_t *label;
    char duration[24];
    char text[192];

    format_duration(
        duration, sizeof(duration),
        workout != NULL ? workout->duration_seconds : 0);
    panel = crazypod_ui_widget_box(
        content, 18, 52, 284, 150, 14, 0x111512, 238);
    snprintf(text, sizeof(text),
             CP_FMT("%s\n\n%04d-%02d-%02d\n%s\n\n"
                    "Time-only workout\nCenter: Delete"),
             workout != NULL
                 ? crazypod_workout_activity_title(workout->activity)
                 : CP_FMT("Missing Workout"),
             workout != NULL ? (int)(workout->date / 10000) : 0,
             workout != NULL ? (int)(workout->date / 100 % 100) : 0,
             workout != NULL ? (int)(workout->date % 100) : 0,
             duration);
    label = crazypod_ui_widget_label(
        panel, text, CRAZYPOD_WORKOUT_FONT,
        CRAZYPOD_WORKOUT_WHITE, 230);
    lv_obj_set_pos(label, 16, 14);
    lv_obj_set_width(label, 252);
}

#endif
