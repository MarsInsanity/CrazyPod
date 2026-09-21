#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../../crazypod_organizer.h"
#include "crazypod_calendar_model.h"
#include "../../../crazypod_mono.h"
#include "../../../crazypod_runtime_font.h"
#include "../../presentation/crazypod_ui_metrics.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_calendar_screen.h"
#include "../../../crazypod_color.h"

#define CALENDAR_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CALENDAR_WHITE 0xFFFFFF
#define CALENDAR_PANEL 0x1B1B22

#ifdef HAVE_CRAZYPOD_COMPACT_UI
/*
 * The event card. A 284x145 panel at 18,54 is most of a 320x240 screen
 * and none of a 138x110 one, and its near-black fill is the page it sits
 * on once the design map has had it.
 */
#define CARD_X 3
#define CARD_Y (CRAZYPOD_METRIC_STATUS_HEIGHT + 2)
#define CARD_WIDTH (LCD_WIDTH - 6)
#define CARD_HEIGHT (LCD_HEIGHT - CARD_Y - 3)
#define CARD_RADIUS 4
#define CARD_TEXT_X 4
#define CARD_TEXT_Y 3
#define CARD_TEXT_WIDTH (CARD_WIDTH - 10)
#define CARD_TEXT_FONT (crazypod_runtime_font_at_size(11))
#else
#define CARD_X 18
#define CARD_Y 54
#define CARD_WIDTH 284
#define CARD_HEIGHT 145
#define CARD_RADIUS 12
#define CARD_TEXT_X 14
#define CARD_TEXT_Y 14
#define CARD_TEXT_WIDTH 256
#define CARD_TEXT_FONT CALENDAR_FONT
#endif

#ifdef HAVE_CRAZYPOD_COMPACT_UI
/* The day sheet: a 270x172 overlay with 27px rows, on a 110px panel. */
#define DAY_X 2
#define DAY_Y (CRAZYPOD_METRIC_STATUS_HEIGHT + 1)
#define DAY_WIDTH (LCD_WIDTH - 4)
#define DAY_HEIGHT (LCD_HEIGHT - DAY_Y - 3)
#define DAY_RADIUS 4
#define DAY_LABEL_X 4
/*
 * No "SCHEDULE" strap. It captions a date that already says what the
 * sheet is, and the pixels it wants are the ones the date row needs so as
 * not to sit on the rule below it.
 */
#define DAY_SHOW_TAG 0
#define DAY_TAG_Y 2
#define DAY_DATE_Y 2
#define DAY_DATE_FONT (&lv_font_montserrat_12)
#define DAY_COUNT_WIDTH 40
#define DAY_COUNT_X (DAY_WIDTH - DAY_LABEL_X - DAY_COUNT_WIDTH)
#define DAY_COUNT_Y 6
#define DAY_RULE_X 4
#define DAY_RULE_Y 19
#define DAY_RULE_WIDTH (DAY_WIDTH - 8)
#define DAY_ROW_Y 23
#define DAY_ROW_STEP 15
#define DAY_ROW_HEIGHT 14
#define DAY_SELECT_X 2
#define DAY_SELECT_WIDTH (DAY_WIDTH - 4)
#define DAY_ICON_X 4
#define DAY_ICON_DY 2
#define DAY_ADD_X 16
#define DAY_TIME_X 4
#define DAY_TIME_WIDTH 28
#define DAY_SEPARATOR_X 34
#define DAY_SEPARATOR_WIDTH 1
#define DAY_SEPARATOR_HEIGHT 11
#define DAY_SUMMARY_X 38
#define DAY_SUMMARY_WIDTH (DAY_WIDTH - 42)
#define DAY_ROW_FONT (&lv_font_montserrat_8)
#else
#define DAY_X 25
#define DAY_Y 49
#define DAY_WIDTH 270
#define DAY_HEIGHT 172
#define DAY_RADIUS 12
#define DAY_LABEL_X 14
#define DAY_SHOW_TAG 1
#define DAY_TAG_Y 5
#define DAY_DATE_Y 22
#define DAY_DATE_FONT (&lv_font_montserrat_16)
#define DAY_COUNT_WIDTH 72
#define DAY_COUNT_X 184
#define DAY_COUNT_Y 29
#define DAY_RULE_X 12
#define DAY_RULE_Y 49
#define DAY_RULE_WIDTH 246
#define DAY_ROW_Y 57
#define DAY_ROW_STEP 27
#define DAY_ROW_HEIGHT 25
#define DAY_SELECT_X 10
#define DAY_SELECT_WIDTH 250
#define DAY_ICON_X 17
#define DAY_ICON_DY 4
#define DAY_ADD_X 43
#define DAY_TIME_X 17
#define DAY_TIME_WIDTH 44
#define DAY_SEPARATOR_X 64
#define DAY_SEPARATOR_WIDTH 2
#define DAY_SEPARATOR_HEIGHT 17
#define DAY_SUMMARY_X 75
#define DAY_SUMMARY_WIDTH 180
#define DAY_ROW_FONT (&lv_font_montserrat_10)
#endif

/*
 * The month sheet is ink on paper like the watch faces, so it names the
 * panel's shades rather than going through the design map, which inverts
 * for the monochrome build and would print the sheet black.
 */
#ifdef HAVE_CRAZYPOD_MONO_UI
#define SHEET_BOX crazypod_ui_widget_box_shade
#define SHEET_LABEL crazypod_ui_widget_label_shade
#define SHEET_INK crazypod_ui_shade
#define SHEET_PAPER 0xFFFFFF
#define SHEET_DARK 0x000000
#define SHEET_MUTED 0x555555
#define SHEET_FAINT 0xAAAAAA
#define SHEET_SELECTED 0xAAAAAA
#else
#define SHEET_BOX crazypod_ui_widget_box
#define SHEET_LABEL crazypod_ui_widget_label
#define SHEET_INK crazypod_ui_color
#define SHEET_PAPER 0xFFFFFF
#define SHEET_DARK 0x0E0E0E
#define SHEET_MUTED 0x5C5C5C
#define SHEET_FAINT 0x949494
#define SHEET_SELECTED 0xF3F3F0
#endif

/*
 * The month grid is 42 cells, each of which was a fresh label plus up to
 * two more objects on every render -- and moving the focus by one day is
 * a render. Building it once and restyling the cells in place turns an
 * arrow press from ~130 object creations into 42 style writes, which is
 * what made scrolling around the month so slow.
 *
 * The pane is cleaned wholesale on a route change, so the pointers are
 * dropped in crazypod_calendar_screen_forget(), which the route renderer
 * calls before that clean; the parent and month are re-checked here as a
 * backstop.
 */
static const char *const calendar_compact_weekdays[] = {
    CP_TR("SUN"), CP_TR("MON"), CP_TR("TUE"), CP_TR("WED"),
    CP_TR("THU"), CP_TR("FRI"), CP_TR("SAT")
};

struct calendar_cell {
    lv_obj_t *highlight;
    lv_obj_t *label;
    lv_obj_t *dot;
};

static struct {
    lv_obj_t *panel;
    int year;
    int month;
    struct calendar_cell cells[42];
    lv_obj_t *day_label;
} grid;

void crazypod_calendar_screen_forget(void)
{
    memset(&grid, 0, sizeof(grid));
}

static bool grid_usable(
    const struct crazypod_calendar_screen_date *date)
{
    int i;

    if(grid.panel == NULL ||
       grid.year != date->year || grid.month != date->month ||
       !lv_obj_is_valid(grid.panel))
        return false;
    for(i = 0; i < 42; ++i) {
        if(grid.cells[i].label == NULL || grid.cells[i].dot == NULL ||
           grid.cells[i].highlight == NULL)
            return false;
    }
    return grid.day_label != NULL;
}

/* Restyles one cell for the focused day; the text never changes while the
 * month does not, so only colour, opacity and visibility move. */
static void refresh_cell(
    struct calendar_cell *cell, bool in_month, bool selected,
    bool is_today, bool has_event)
{
    if(selected) {
        lv_obj_set_style_bg_opa(cell->highlight, LV_OPA_COVER, 0);
        lv_obj_set_style_border_opa(cell->highlight, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(cell->highlight, LV_OBJ_FLAG_HIDDEN);
    }
    else if(is_today) {
        lv_obj_set_style_bg_opa(cell->highlight, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_opa(cell->highlight, 210, 0);
        lv_obj_remove_flag(cell->highlight, LV_OBJ_FLAG_HIDDEN);
    }
    else
        lv_obj_add_flag(cell->highlight, LV_OBJ_FLAG_HIDDEN);

    /*
     * The sheet's own shades, as at creation. Through the design map the
     * near-black day number becomes paper, which is the colour it is
     * printed on: the month came out blank but for the days either side
     * of it, which map the other way.
     */
    lv_obj_set_style_text_color(
        cell->label,
        SHEET_INK(selected ? SHEET_PAPER :
                  in_month ? SHEET_DARK : SHEET_FAINT), 0);
    lv_obj_set_style_text_opa(
        cell->label, in_month ? LV_OPA_COVER : 155, 0);

    if(has_event) {
        lv_obj_set_style_bg_color(
            cell->dot,
            SHEET_INK(selected ? SHEET_PAPER : SHEET_DARK), 0);
        lv_obj_set_style_bg_opa(
            cell->dot, in_month ? 205 : 70, 0);
        lv_obj_remove_flag(cell->dot, LV_OBJ_FLAG_HIDDEN);
    }
    else
        lv_obj_add_flag(cell->dot, LV_OBJ_FLAG_HIDDEN);
}

bool crazypod_calendar_screen_refocus(
    const struct crazypod_calendar_screen_date *date)
{
    char text[48];
    int first;
    int count;
    int previous_month;
    int previous_year;
    int previous_count;
    int slot;

    if(!grid_usable(date))
        return false;

    first = crazypod_ui_calendar_weekday(date->year, date->month, 1);
    count = crazypod_ui_calendar_days_in_month(date->year, date->month);
    previous_month = date->month - 1;
    previous_year = date->year;
    if(previous_month < 0) {
        previous_month = 11;
        --previous_year;
    }
    previous_count = crazypod_ui_calendar_days_in_month(
        previous_year, previous_month);

    snprintf(text, sizeof(text), CP_FMT("%s %d"),
             calendar_compact_weekdays[
                 crazypod_ui_calendar_weekday(
                     date->year, date->month, date->day)],
             date->day);
    CP_LV_LABEL_SET_TEXT(grid.day_label, text);

    for(slot = 0; slot < 42; ++slot) {
        int relative_day = slot - first + 1;
        int day = relative_day;
        int year = date->year;
        int month = date->month;
        bool in_month = true;
        int cell_date;
        int event_index;
        bool has_event = false;

        if(relative_day < 1) {
            day = previous_count + relative_day;
            year = previous_year;
            month = previous_month;
            in_month = false;
        }
        else if(relative_day > count) {
            day = relative_day - count;
            if(++month > 11) {
                month = 0;
                ++year;
            }
            in_month = false;
        }
        cell_date = year * 10000 + (month + 1) * 100 + day;
        for(event_index = 0;
            event_index < crazypod_calendar_event_count();
            ++event_index) {
            const struct crazypod_calendar_event *event =
                crazypod_calendar_event_get(event_index);
            if(event != NULL && event->date == cell_date) {
                has_event = true;
                break;
            }
        }
        refresh_cell(&grid.cells[slot], in_month,
                     in_month && day == date->day,
                     cell_date == date->today, has_event);
    }
    return true;
}

void crazypod_calendar_screen_render_grid(
    lv_obj_t *content,
    const struct crazypod_calendar_screen_date *date)
{
    static const char *const months[] = {
        CP_TR("January"), CP_TR("February"), CP_TR("March"), CP_TR("April"), CP_TR("May"), CP_TR("June"),
        CP_TR("July"), CP_TR("August"), CP_TR("September"), CP_TR("October"), CP_TR("November"), CP_TR("December")
    };
    static const char *const weekdays[] = {
        CP_TR("S"), CP_TR("M"), CP_TR("T"), CP_TR("W"), CP_TR("T"), CP_TR("F"), CP_TR("S")
    };
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[48];
    int first = crazypod_ui_calendar_weekday(
        date->year, date->month, 1);
    int count = crazypod_ui_calendar_days_in_month(
        date->year, date->month);
    int previous_month = date->month - 1;
    int previous_year = date->year;
    int previous_count;
    int slot;

    if(previous_month < 0) {
        previous_month = 11;
        --previous_year;
    }
    previous_count = crazypod_ui_calendar_days_in_month(
        previous_year, previous_month);

    if(crazypod_calendar_screen_refocus(date))
        return;

    memset(&grid, 0, sizeof(grid));
    SHEET_BOX(
        content, 0, CRAZYPOD_METRIC_STATUS_HEIGHT, LCD_WIDTH,
        LCD_HEIGHT - CRAZYPOD_METRIC_STATUS_HEIGHT, 0,
        SHEET_PAPER, LV_OPA_COVER);
    panel = SHEET_BOX(
        content, CRAZYPOD_METRIC_CAL_PANEL_X,
        CRAZYPOD_METRIC_CAL_PANEL_Y,
        CRAZYPOD_METRIC_CAL_PANEL_WIDTH,
        CRAZYPOD_METRIC_CAL_PANEL_HEIGHT,
        CRAZYPOD_METRIC_CAL_PANEL_RADIUS, SHEET_PAPER, LV_OPA_COVER);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, SHEET_INK(SHEET_DARK), 0);
    lv_obj_set_style_border_opa(panel, 90, 0);
#if CRAZYPOD_METRIC_CAL_SHOW_CAPTION
    label = SHEET_LABEL(
        panel, CP_TR("CALENDAR"), &lv_font_montserrat_8,
        SHEET_FAINT, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, 14, 5);
#endif
    snprintf(text, sizeof(text), CP_FMT("%s %d"),
             months[date->month], date->year);
    label = SHEET_LABEL(
        panel, text,
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_CAL_TITLE_SIZE),
        SHEET_DARK, LV_OPA_COVER);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_CAL_TITLE_X,
                   CRAZYPOD_METRIC_CAL_TITLE_Y);
    snprintf(text, sizeof(text), CP_FMT("%s %d"),
             calendar_compact_weekdays[
                 crazypod_ui_calendar_weekday(
                     date->year, date->month, date->day)],
             date->day);
    label = SHEET_LABEL(
        panel, text, &lv_font_montserrat_10,
        SHEET_MUTED, LV_OPA_COVER);
    lv_obj_set_width(label, CRAZYPOD_METRIC_CAL_DAY_LABEL_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, CRAZYPOD_METRIC_CAL_DAY_LABEL_X,
                   CRAZYPOD_METRIC_CAL_DAY_LABEL_Y);
    grid.day_label = label;
    SHEET_BOX(
        panel, CRAZYPOD_METRIC_CAL_RULE_X, CRAZYPOD_METRIC_CAL_RULE_Y,
        CRAZYPOD_METRIC_CAL_RULE_WIDTH, 1, 0, SHEET_DARK, 205);

    for(slot = 0; slot < 7; ++slot) {
        label = SHEET_LABEL(
            panel, weekdays[slot], &lv_font_montserrat_8,
            SHEET_FAINT, 230);
        lv_obj_set_width(label, CRAZYPOD_METRIC_CAL_CELL_WIDTH);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(
            label,
            CRAZYPOD_METRIC_CAL_GRID_X +
                slot * CRAZYPOD_METRIC_CAL_CELL_WIDTH,
            CRAZYPOD_METRIC_CAL_WEEKDAY_Y);
    }
    for(slot = 0; slot < 42; ++slot) {
        int column = slot % 7;
        int row = slot / 7;
        int x = CRAZYPOD_METRIC_CAL_GRID_X +
                column * CRAZYPOD_METRIC_CAL_CELL_WIDTH;
        int y = CRAZYPOD_METRIC_CAL_GRID_Y +
                row * CRAZYPOD_METRIC_CAL_CELL_HEIGHT;
        int relative_day = slot - first + 1;
        int day;
        int year = date->year;
        int month = date->month;
        bool in_month = true;
        int cell_date;
        bool selected;
        bool is_today;
        bool has_event = false;
        int event_index;
        char day_text[4];

        if(relative_day < 1) {
            day = previous_count + relative_day;
            year = previous_year;
            month = previous_month;
            in_month = false;
        }
        else if(relative_day > count) {
            day = relative_day - count;
            if(++month > 11) {
                month = 0;
                ++year;
            }
            in_month = false;
        }
        else
            day = relative_day;
        cell_date = year * 10000 + (month + 1) * 100 + day;
        selected = in_month && day == date->day;
        is_today = cell_date == date->today;
        for(event_index = 0;
            event_index < crazypod_calendar_event_count();
            ++event_index) {
            const struct crazypod_calendar_event *event =
                crazypod_calendar_event_get(event_index);
            if(event != NULL && event->date == cell_date) {
                has_event = true;
                break;
            }
        }
        /* Every cell gets all three objects whatever its state, so a
         * later focus move only has to restyle them. */
        grid.cells[slot].highlight = SHEET_BOX(
            panel, x + 1, y - 1,
            CRAZYPOD_METRIC_CAL_CELL_WIDTH - 2,
            CRAZYPOD_METRIC_CAL_CELL_HEIGHT,
            CRAZYPOD_METRIC_CAL_CELL_HEIGHT / 3,
            SHEET_DARK, LV_OPA_COVER);
        lv_obj_set_style_border_width(
            grid.cells[slot].highlight, 1, 0);
        lv_obj_set_style_border_color(
            grid.cells[slot].highlight, SHEET_INK(SHEET_DARK), 0);

        snprintf(day_text, sizeof(day_text), CP_FMT("%d"), day);
        label = SHEET_LABEL(
            panel, day_text,
            crazypod_runtime_font_at_size(CRAZYPOD_METRIC_CAL_DAY_SIZE),
            SHEET_DARK, LV_OPA_COVER);
        lv_obj_set_width(label, CRAZYPOD_METRIC_CAL_CELL_WIDTH);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, x, y);
        grid.cells[slot].label = label;

        /* The event dot sits in the cell's bottom corner; on the small
         * grid that corner is four pixels from the number. */
        grid.cells[slot].dot = SHEET_BOX(
            panel, x + CRAZYPOD_METRIC_CAL_CELL_WIDTH / 2 - 1,
            y + CRAZYPOD_METRIC_CAL_CELL_HEIGHT - 4,
            CRAZYPOD_METRIC_CAL_DOT, CRAZYPOD_METRIC_CAL_DOT,
            LV_RADIUS_CIRCLE, SHEET_DARK, 205);

        refresh_cell(&grid.cells[slot], in_month, selected,
                     is_today, has_event);
    }

    grid.panel = panel;
    grid.year = date->year;
    grid.month = date->month;
    lv_obj_null_on_delete(&grid.panel);
    for(slot = 0; slot < 42; ++slot) {
        lv_obj_null_on_delete(&grid.cells[slot].highlight);
        lv_obj_null_on_delete(&grid.cells[slot].label);
        lv_obj_null_on_delete(&grid.cells[slot].dot);
    }
    lv_obj_null_on_delete(&grid.day_label);
}

void crazypod_calendar_screen_render_day(
    lv_obj_t *content,
    const struct crazypod_calendar_screen_date *date,
    const struct crazypod_calendar_screen_events *events)
{
    lv_obj_t *overlay;
    lv_obj_t *label;
    char text[64];
    int start = events->selected > 3 ? events->selected - 3 : 0;
    int row;

    crazypod_calendar_screen_render_grid(content, date);
    /* The same sheet shades the month grid names; see SHEET_BOX above. */
    overlay = SHEET_BOX(
        content, DAY_X, DAY_Y, DAY_WIDTH, DAY_HEIGHT, DAY_RADIUS,
        SHEET_PAPER, LV_OPA_COVER);
    lv_obj_set_style_border_width(overlay, 1, 0);
    lv_obj_set_style_border_color(overlay, SHEET_INK(SHEET_DARK), 0);
    lv_obj_set_style_border_opa(overlay, 210, 0);
#if DAY_SHOW_TAG
    label = SHEET_LABEL(
        overlay, CP_TR("SCHEDULE"), &lv_font_montserrat_8,
        SHEET_FAINT, LV_OPA_COVER);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    lv_obj_set_pos(label, DAY_LABEL_X, DAY_TAG_Y);
#endif
    snprintf(text, sizeof(text), CP_FMT("%04d-%02d-%02d"),
             date->year, date->month + 1, date->day);
    label = SHEET_LABEL(
        overlay, text, DAY_DATE_FONT,
        SHEET_DARK, LV_OPA_COVER);
    lv_obj_set_pos(label, DAY_LABEL_X, DAY_DATE_Y);
    snprintf(text, sizeof(text), CP_FMT("%d item%s"),
             events->count, events->count == 1 ? "" : "s");
    label = SHEET_LABEL(
        overlay, text, &lv_font_montserrat_8, SHEET_MUTED, 220);
    lv_obj_set_width(label, DAY_COUNT_WIDTH);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(label, DAY_COUNT_X, DAY_COUNT_Y);
    SHEET_BOX(
        overlay, DAY_RULE_X, DAY_RULE_Y, DAY_RULE_WIDTH, 1, 0,
        SHEET_DARK, 205);

    for(row = 0; row < 4; ++row) {
        int position = start + row;
        const struct crazypod_calendar_event *event;
        int y = DAY_ROW_Y + row * DAY_ROW_STEP;
        bool selected;

        if(position > events->count)
            break;
        selected = position == events->selected;
        if(selected)
            SHEET_BOX(
                overlay, DAY_SELECT_X, y - 2,
                DAY_SELECT_WIDTH, DAY_ROW_HEIGHT, 5,
                SHEET_SELECTED, LV_OPA_COVER);
        if(position == events->count) {
            label = SHEET_LABEL(
                overlay, LV_SYMBOL_EDIT, DAY_ROW_FONT,
                SHEET_DARK, 220);
            lv_obj_set_pos(label, DAY_ICON_X, y + DAY_ICON_DY);
            label = SHEET_LABEL(
                overlay, CP_TR("Add Event"), DAY_ROW_FONT,
                SHEET_DARK, LV_OPA_COVER);
            lv_obj_set_pos(label, DAY_ADD_X, y + DAY_ICON_DY);
            continue;
        }
        event = crazypod_calendar_event_get(
            events->index_at(events->context, position));
        if(event == NULL)
            continue;
        label = SHEET_LABEL(
            overlay,
            event->time[0] != '\0' ? event->time : CP_TR("All day"),
            &lv_font_montserrat_8, SHEET_MUTED, 230);
        lv_obj_set_width(label, DAY_TIME_WIDTH);
        lv_obj_set_pos(label, DAY_TIME_X, y + DAY_ICON_DY);
        SHEET_BOX(
            overlay, DAY_SEPARATOR_X, y + 1,
            DAY_SEPARATOR_WIDTH, DAY_SEPARATOR_HEIGHT, 1,
            SHEET_DARK, 210);
        label = SHEET_LABEL(
            overlay, event->summary, DAY_ROW_FONT,
            SHEET_DARK, LV_OPA_COVER);
        lv_obj_set_width(label, DAY_SUMMARY_WIDTH);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_pos(label, DAY_SUMMARY_X, y + DAY_ICON_DY);
    }
}

void crazypod_calendar_screen_render_detail(
    lv_obj_t *content, int event_index)
{
    const struct crazypod_calendar_event *event =
        crazypod_calendar_event_get(event_index);
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[180];

#ifdef HAVE_CRAZYPOD_MONO_UI
    panel = crazypod_ui_widget_box_shade(
        content, CARD_X, CARD_Y, CARD_WIDTH, CARD_HEIGHT,
        CARD_RADIUS, CRAZYPOD_MONO_SHADE_PALE, LV_OPA_COVER);
#else
    panel = crazypod_ui_widget_box(
        content, CARD_X, CARD_Y, CARD_WIDTH, CARD_HEIGHT,
        CARD_RADIUS, CALENDAR_PANEL, 230);
#endif
    snprintf(text, sizeof(text),
             CP_FMT("%s\n\n%04d-%02d-%02d\n%s\n\n%s"),
             event != NULL ? event->summary : CP_FMT("No Event"),
             event != NULL ? event->date / 10000 : 0,
             event != NULL ? event->date / 100 % 100 : 0,
             event != NULL ? event->date % 100 : 0,
             event != NULL && event->time[0] != '\0'
                ? event->time : CP_FMT("All day"),
             event != NULL && event->editable
                ? CP_FMT("Center: Event Actions")
                : CP_FMT("Imported from .ics"));
#ifdef HAVE_CRAZYPOD_MONO_UI
    label = crazypod_ui_widget_label_shade(
        panel, text, CARD_TEXT_FONT,
        CRAZYPOD_MONO_INK, LV_OPA_COVER);
#else
    label = crazypod_ui_widget_label(
        panel, text, CARD_TEXT_FONT, CALENDAR_WHITE, 235);
#endif
    lv_obj_set_pos(label, CARD_TEXT_X, CARD_TEXT_Y);
    lv_obj_set_width(label, CARD_TEXT_WIDTH);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}

#endif
