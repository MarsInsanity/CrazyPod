#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "crazypod_collation.h"
#include "features/organizer/crazypod_calendar_model.h"
#include "presentation/crazypod_ui_menu_layout.h"
#include "presentation/crazypod_scene_motion.h"
#include "presentation/crazypod_panel_geometry.h"
#include "presentation/crazypod_ui_text.h"
#include "features/crazypod_feature.h"
#include "navigation/crazypod_feature_dispatcher.h"
#include "navigation/crazypod_alpha_jump.h"
#include "navigation/crazypod_wheel_accel.h"
#include "navigation/crazypod_navigation_command.h"
#include "navigation/crazypod_route_registry.h"

const char *crazypod_l10n_text(const char *text)
{
    return text != NULL && text[0] == '\x1f' ? text + 1 : text;
}

static void test_calendar(void)
{
    char time[16];

    assert(crazypod_ui_calendar_days_in_month(2024, 1) == 29);
    assert(crazypod_ui_calendar_days_in_month(2100, 1) == 28);
    assert(crazypod_ui_calendar_weekday(2026, 6, 28) == 2);
    assert(crazypod_ui_calendar_shift_date(20241231, 1) == 20250101);
    assert(crazypod_ui_calendar_shift_date(20240301, -1) == 20240229);
    assert(crazypod_ui_calendar_parse_minutes("09:30") == 570);
    assert(crazypod_ui_calendar_parse_minutes("bad") == -1);
    crazypod_ui_calendar_format_time(time, sizeof(time), 570, false);
    assert(strcmp(time, "09:30") == 0);
    crazypod_ui_calendar_format_time(time, sizeof(time), 570, true);
    assert(strcmp(time, "9:30 AM") == 0);
}

/*
 * A nine-hour audiobook is what broke the hand-written version of this:
 * 281 * elapsed overflows a 32-bit unsigned at 4.25 hours, so the bar
 * wrapped and drew the last chapter near the start.
 */
static void test_bar_fill(void)
{
    const uint32_t book = 33471471;   /* 9.3 hours, from a real file */

    assert(crazypod_ui_text_bar_fill(281, 0, book) == 0);
    assert(crazypod_ui_text_bar_fill(281, book, book) == 281);
    assert(crazypod_ui_text_bar_fill(281, book + 1, book) == 281);
    /* Just under and just over where 32-bit arithmetic gives up. */
    assert(crazypod_ui_text_bar_fill(281, 15284581, book) == 128);
    assert(crazypod_ui_text_bar_fill(281, 15284582, book) == 128);
    /* The two positions that were reported wrong on the device. */
    assert(crazypod_ui_text_bar_fill(281, 30000000, book) == 251);
    assert(crazypod_ui_text_bar_fill(281, 32270814, book) == 270);
    /* The capsule's narrower bar overflows later but still overflows. */
    assert(crazypod_ui_text_bar_fill(171, 32270814, book) == 164);
    /* Degenerate inputs must not divide by zero or return rubbish. */
    assert(crazypod_ui_text_bar_fill(281, 1000, 0) == 0);
    assert(crazypod_ui_text_bar_fill(0, 1000, book) == 0);
}

/*
 * The home capsule and the lock screen media widget are split in two so each
 * half can take its own screen corner. Each half clips a layer the height of
 * the whole panel; the seam is only invisible while the edge the half does
 * not own falls outside its clip. At a corner radius of zero the two borders
 * landed either side of the split and drew a line across the middle of the
 * widget, which is what a square-cornered iPod showed.
 */
static void test_panel_half_geometry(void)
{
    struct crazypod_panel_half top;
    struct crazypod_panel_half bottom;
    int radius;

    /* Square corners: the reported bug. Both borders must clear the split. */
    crazypod_panel_half_geometry(94, 0, true, &top);
    crazypod_panel_half_geometry(94, 0, false, &bottom);
    assert(top.clip_y == 0 && top.clip_height == 47);
    assert(bottom.clip_y == 47 && bottom.clip_height == 47);
    /* The top half's bottom border row, past the last row it can draw. */
    assert(top.border_y + top.border_height - 1 > top.clip_height - 1);
    /* The bottom half's top border row, above the first row it can draw. */
    assert(bottom.border_y < 0);

    /* The default eight-pixel corner, and every radius in between. */
    for(radius = 0; radius <= 47; ++radius) {
        crazypod_panel_half_geometry(94, radius, true, &top);
        crazypod_panel_half_geometry(94, radius, false, &bottom);
        assert(top.border_y + top.border_height - 1 > top.clip_height - 1);
        assert(bottom.border_y < 0);
        /* The far edge of each half still lands on the panel's own edge. */
        assert(top.border_y == 0);
        assert(bottom.border_y + bottom.border_height - 1 ==
               bottom.clip_height - 1);
        /* The material keeps the radius it was given, either side of it. */
        assert(top.layer_height == top.clip_height + radius);
        assert(bottom.layer_y == -radius);
        assert(bottom.glass_y == radius - 47);
    }

    /* An odd height gives the bottom half the extra row, and no gap. */
    crazypod_panel_half_geometry(95, 8, true, &top);
    crazypod_panel_half_geometry(95, 8, false, &bottom);
    assert(top.clip_height == 47 && bottom.clip_height == 48);
    assert(bottom.clip_y == top.clip_y + top.clip_height);

    /* A radius larger than the panel cannot push the border back over it. */
    crazypod_panel_half_geometry(94, 400, false, &bottom);
    assert(bottom.border_y < 0 && bottom.layer_y >= -47);
}

/* Midnight and noon are where hand-rolled twelve-hour clocks go wrong. */
static void test_clock_format(void)
{
    char text[24];

    assert(strcmp(crazypod_ui_text_clock(text, sizeof(text),
                                         0, 5, 0, false, false),
                  "00:05") == 0);
    assert(strcmp(crazypod_ui_text_clock(text, sizeof(text),
                                         0, 5, 0, false, true),
                  "12:05 AM") == 0);
    assert(strcmp(crazypod_ui_text_clock(text, sizeof(text),
                                         12, 0, 0, false, true),
                  "12:00 PM") == 0);
    assert(strcmp(crazypod_ui_text_clock(text, sizeof(text),
                                         11, 59, 0, false, true),
                  "11:59 AM") == 0);
    assert(strcmp(crazypod_ui_text_clock(text, sizeof(text),
                                         13, 7, 9, true, true),
                  "1:07:09 PM") == 0);
    assert(strcmp(crazypod_ui_text_clock(text, sizeof(text),
                                         23, 59, 59, true, false),
                  "23:59:59") == 0);
}

static void test_note_layout(void)
{
    char window[128];
    const char *body =
        "1234567890123456789012345678901234"
        "second line";

    assert(crazypod_ui_text_note_line_count("") == 1);
    assert(crazypod_ui_text_note_line_count(body) == 2);
    crazypod_ui_text_note_window(body, 1, window, sizeof(window));
    assert(strcmp(window, "second line") == 0);
}

static void test_editor(void)
{
    char text[16] = "ab";
    char cursor_text[16];
    size_t cursor = 1;

    crazypod_ui_text_insert(text, sizeof(text), &cursor, "中");
    assert(strcmp(text, "a中b") == 0);
    assert(cursor == 4);
    crazypod_ui_text_backspace_at(text, &cursor);
    assert(strcmp(text, "ab") == 0);
    assert(cursor == 1);

    crazypod_ui_text_append(text, sizeof(text), "文");
    assert(strcmp(text, "ab文") == 0);
    crazypod_ui_text_backspace(text);
    assert(strcmp(text, "ab") == 0);

    cursor = strlen("a中");
    crazypod_ui_text_move_cursor("a中b", &cursor, -1);
    assert(cursor == 1);
    crazypod_ui_text_move_cursor("a中b", &cursor, 1);
    assert(cursor == strlen("a中"));

    assert(strcmp(crazypod_ui_text_with_cursor(
                      "abc", 1, cursor_text, sizeof(cursor_text)),
                  "a|bc") == 0);
}

static void test_menu_layout(void)
{
    int thumb_y;
    int thumb_height;

    assert(crazypod_ui_menu_window_start(4, 3, 7) == 0);
    assert(crazypod_ui_menu_window_start(20, 0, 7) == 0);
    assert(crazypod_ui_menu_window_start(20, 10, 7) == 7);
    assert(crazypod_ui_menu_window_start(20, 19, 7) == 13);
    crazypod_ui_menu_scroll_thumb(
        20, 19, 7, 66, 164, 12,
        &thumb_y, &thumb_height);
    assert(thumb_height == 57);
    assert(thumb_y == 173);
}

static const char *section_title_at(
    int index, void *context)
{
    const char *const *titles = context;

    return titles[index];
}

static void test_collation(void)
{
    static const char *const titles[] = {
        "1 Song", "Alice", "Another", "北京",
        "重庆", "上海"
    };
    int target;
    char key;

    assert(crazypod_collation_initial(" Alice") == 'A');
    assert(crazypod_collation_initial("9 Songs") == '#');
    assert(crazypod_collation_initial("Édith") == 'E');
    assert(crazypod_collation_initial("愛") == 'A');
    assert(crazypod_collation_initial("北京") == 'B');
    assert(crazypod_collation_initial("重庆") == 'Z');
    assert(crazypod_collation_compare("9 Songs", "Alice") < 0);
    assert(crazypod_collation_compare("北京", "重庆") < 0);

    assert(crazypod_collation_section_target(
        6, 1, 1, section_title_at,
        (void *)titles, &target, &key));
    assert(target == 3);
    assert(key == 'B');
    assert(crazypod_collation_section_target(
        6, 5, -1, section_title_at,
        (void *)titles, &target, &key));
    assert(target == 4);
    assert(key == 'Z');
    assert(!crazypod_collation_section_target(
        6, 0, -1, section_title_at,
        (void *)titles, &target, &key));
}

static void test_alpha_jump_burst(void)
{
    struct crazypod_alpha_jump_state state;

    crazypod_alpha_jump_reset(&state);
    assert(!crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_SONGS, -1,
        1, 3, 100, 32, 7, 1));
    assert(crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_SONGS, -1,
        1, 4, 110, 32, 7, 1));
    assert(crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_SONGS, -1,
        1, 1, 120, 32, 7, 1));
    assert(!crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_SONGS, -1,
        -1, 1, 121, 32, 7, 1));
    assert(!crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_SONGS, -1,
        -1, 6, 200, 32, 7, 1));
    assert(!crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_ARTISTS, -1,
        -1, 1, 201, 32, 7, 1));
}

/*
 * One flick of the wheel reports many steps at once. However far it turns,
 * a single event must never reach the letter jump.
 */
static void test_alpha_jump_needs_sustained_spin(void)
{
    struct crazypod_alpha_jump_state state;
    int i;

    crazypod_alpha_jump_reset(&state);
    assert(!crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_SONGS, -1,
        1, 12, 100, 32, 24, 4));
    assert(!crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_SONGS, -1,
        1, 12, 110, 32, 24, 4));
    /* Steps are there from the third event on; the event floor is not. */
    assert(!crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_SONGS, -1,
        1, 12, 120, 32, 24, 4));
    assert(crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_SONGS, -1,
        1, 12, 130, 32, 24, 4));

    /* Enough events, but a slow turn never reaches the step threshold. */
    crazypod_alpha_jump_reset(&state);
    for(i = 0; i < 8; ++i)
        assert(!crazypod_alpha_jump_consume(
            &state, MUSIC_ROUTE_SONGS, -1,
            1, 1, 100 + i * 10, 32, 24, 4));

    /* A pause longer than the window starts the count over. */
    crazypod_alpha_jump_reset(&state);
    for(i = 0; i < 3; ++i)
        assert(!crazypod_alpha_jump_consume(
            &state, MUSIC_ROUTE_SONGS, -1,
            1, 12, 100 + i * 10, 32, 24, 4));
    assert(!crazypod_alpha_jump_consume(
        &state, MUSIC_ROUTE_SONGS, -1,
        1, 12, 400, 32, 24, 4));
}

/*
 * The wheel step grows with how fast the wheel is turning, measured in
 * clicks per second, and not with how slow the screen happens to be.
 */
static void test_wheel_accel_slow_turn_stays_single(void)
{
    struct crazypod_wheel_accel_state state;
    int i;

    crazypod_wheel_accel_reset(&state);
    /* One click every 20 ticks is five clicks a second: a careful turn. */
    for(i = 0; i < 12; ++i)
        assert(crazypod_wheel_accel_step(
            &state, 1, 1, 100 + i * 20, 32, 100, 12) == 1);
}

static void test_wheel_accel_fast_spin_multiplies(void)
{
    struct crazypod_wheel_accel_state state;
    int step = 0;
    int i;

    crazypod_wheel_accel_reset(&state);
    /* Four clicks every 4 ticks is a hundred a second: a hard spin. */
    for(i = 0; i < 12; ++i)
        step = crazypod_wheel_accel_step(
            &state, 1, 4, 100 + i * 4, 32, 100, 12);
    assert(step > 4);
    assert(step <= 12);
    /* And with headroom it is the multiplier, not the cap, doing it. */
    crazypod_wheel_accel_reset(&state);
    for(i = 0; i < 12; ++i)
        step = crazypod_wheel_accel_step(
            &state, 1, 4, 100 + i * 4, 32, 100, 96);
    assert(step >= 4 * 4);
}

/*
 * The reported fault: spinning as hard as the wheel can be spun still
 * crawled. One click an event at forty clicks a second is a full spin, and
 * it has to be worth more than the six it used to be worth.
 */
static void test_wheel_accel_a_full_spin_crosses_a_long_list(void)
{
    struct crazypod_wheel_accel_state state;
    int step = 0;
    int i;

    crazypod_wheel_accel_reset(&state);
    /* Two clicks every five ticks is forty a second. */
    for(i = 0; i < 16; ++i)
        step = crazypod_wheel_accel_step(
            &state, 1, 2, 100 + i * 5, 32, 100, 40);
    assert(step == 32);
    /* A steady middling turn is quicker than it was, and still not a leap. */
    crazypod_wheel_accel_reset(&state);
    for(i = 0; i < 16; ++i)
        step = crazypod_wheel_accel_step(
            &state, 1, 1, 100 + i * 5, 32, 100, 40);
    assert(step == 4);
    /* The ceiling is still the ceiling, however hard the wheel is turned. */
    crazypod_wheel_accel_reset(&state);
    for(i = 0; i < 16; ++i)
        step = crazypod_wheel_accel_step(
            &state, 1, 8, 100 + i * 2, 32, 100, 40);
    assert(step == 40);
}

static void test_wheel_accel_a_slow_screen_does_not_accelerate(void)
{
    struct crazypod_wheel_accel_state slow;
    struct crazypod_wheel_accel_state fast;
    int slow_step = 0;
    int fast_step = 0;
    int i;

    /*
     * The same hand speed -- eight clicks a second -- reported as one click
     * every 12 ticks, or as eight clicks every 96 ticks because the screen
     * was busy. Both must move by exactly the clicks they were given and
     * accelerate not at all: coalescing is not a faster spin. The busy one
     * moves further per event because it was handed more of the turn, which
     * is the turn being honoured rather than acceleration.
     */
    crazypod_wheel_accel_reset(&slow);
    crazypod_wheel_accel_reset(&fast);
    for(i = 0; i < 10; ++i) {
        fast_step = crazypod_wheel_accel_step(
            &fast, 1, 1, 100 + i * 12, 32, 100, 96);
        slow_step = crazypod_wheel_accel_step(
            &slow, 1, 8, 100 + i * 96, 200, 100, 96);
    }
    assert(fast_step == 1);
    assert(slow_step == 8);
}

static void test_wheel_accel_pause_starts_from_rest(void)
{
    struct crazypod_wheel_accel_state state;
    int i;

    crazypod_wheel_accel_reset(&state);
    for(i = 0; i < 12; ++i)
        (void)crazypod_wheel_accel_step(
            &state, 1, 4, 100 + i * 4, 32, 100, 12);
    /* A gap longer than the window, then one careful click. */
    assert(crazypod_wheel_accel_step(
        &state, 1, 1, 1000, 32, 100, 12) == 1);
}

static void test_wheel_accel_turning_back_starts_from_rest(void)
{
    struct crazypod_wheel_accel_state state;
    int i;

    crazypod_wheel_accel_reset(&state);
    for(i = 0; i < 12; ++i)
        (void)crazypod_wheel_accel_step(
            &state, 1, 4, 100 + i * 4, 32, 100, 12);
    assert(crazypod_wheel_accel_step(
        &state, -1, 1, 152, 32, 100, 12) == 1);
}

static void test_wheel_accel_respects_the_maximum(void)
{
    struct crazypod_wheel_accel_state state;
    int i;
    int step = 0;

    crazypod_wheel_accel_reset(&state);
    for(i = 0; i < 20; ++i)
        step = crazypod_wheel_accel_step(
            &state, 1, 12, 100 + i, 32, 100, 3);
    assert(step == 3);
}

static void test_route_registry(void)
{
    int route;

    assert(crazypod_route_registry_validate());
    for(route = 0; route < CRAZYPOD_ROUTE_COUNT; ++route)
        assert(crazypod_route_registry_get(route) != NULL);
    assert(crazypod_route_registry_is_shell(EXTRAS_ROUTE_MENU));
    assert(crazypod_route_registry_feature(MUSIC_ROUTE_MENU)->id ==
           CRAZYPOD_FEATURE_MUSIC);
    assert(crazypod_route_registry_feature(MUSIC_ROUTE_QUEUE)->id ==
           CRAZYPOD_FEATURE_NOW_PLAYING);
    assert(crazypod_route_registry_feature(
               DIY_ROUTE_WALLPAPER_CROP)->id ==
           CRAZYPOD_FEATURE_CUSTOMIZE);
    assert(crazypod_route_registry_feature(
               DIY_ROUTE_HEADPHONE_POPUP)->id ==
           CRAZYPOD_FEATURE_CUSTOMIZE);
    assert(crazypod_route_registry_feature(MINIAPP_ROUTE_VIEW)->id ==
           CRAZYPOD_FEATURE_MINIAPPS);
    assert(crazypod_route_registry_feature(GAMEBOY_ROUTE_LIBRARY)->id ==
           CRAZYPOD_FEATURE_MINIAPPS);
    assert(crazypod_route_registry_has_flag(
        CLOCK_ROUTE_VIEW, CRAZYPOD_ROUTE_FLAG_FULLSCREEN));
    assert(crazypod_route_registry_has_flag(
        CLOCK_ROUTE_VIEW, CRAZYPOD_ROUTE_FLAG_DARK_STATUS));
    assert(crazypod_route_registry_has_flag(
        DIY_ROUTE_WALLPAPER_CROP,
        CRAZYPOD_ROUTE_FLAG_SOLID_BLACK));
    assert(crazypod_route_registry_has_flag(
        BOOKS_ROUTE_READER, CRAZYPOD_ROUTE_FLAG_BOOK_READER));
    assert(!crazypod_route_registry_has_flag(
        MUSIC_ROUTE_MENU, CRAZYPOD_ROUTE_FLAG_FULLSCREEN));
}

static void test_navigation_commands(void)
{
    struct crazypod_navigation_command command =
        crazypod_navigation_push(NOTES_ROUTE_READER, 7, 3);

    assert(command.kind == CRAZYPOD_NAVIGATION_PUSH);
    assert(command.route == NOTES_ROUTE_READER);
    assert(command.group == 7);
    assert(command.selected == 3);
    assert(command.transition);

    command = crazypod_navigation_render(false);
    assert(command.kind == CRAZYPOD_NAVIGATION_RENDER);
    assert(!command.transition);
    assert(crazypod_navigation_pop().kind == CRAZYPOD_NAVIGATION_POP);
    assert(crazypod_navigation_none().kind == CRAZYPOD_NAVIGATION_NONE);
}

static void test_scene_motion(void)
{
    struct crazypod_scene_motion_layout start;
    struct crazypod_scene_motion_layout middle;
    struct crazypod_scene_motion_layout end;

    crazypod_scene_motion_layout(
        CRAZYPOD_SCENE_MOTION_PUSH, 0, 240, &start);
    crazypod_scene_motion_layout(
        CRAZYPOD_SCENE_MOTION_PUSH, 512, 240, &middle);
    crazypod_scene_motion_layout(
        CRAZYPOD_SCENE_MOTION_PUSH, 1024, 240, &end);
    assert(start.from_x == 0);
    assert(start.from_y == 0);
    assert(start.to_y == 240);
    assert(end.to_y == 0);
    assert(end.from_y == 0);
    assert(middle.to_y < 120);
    assert(start.edge_shadow_opacity > end.edge_shadow_opacity);

    crazypod_scene_motion_layout(
        CRAZYPOD_SCENE_MOTION_POP, 0, 240, &start);
    crazypod_scene_motion_layout(
        CRAZYPOD_SCENE_MOTION_POP, 1024, 240, &end);
    assert(start.from_x == 0);
    assert(start.from_y == 0);
    assert(start.to_y == 0);
    assert(end.from_y == 240);
    assert(end.to_y == 0);

    crazypod_scene_motion_layout(
        CRAZYPOD_SCENE_MOTION_REPLACE, 0, 240, &start);
    crazypod_scene_motion_layout(
        CRAZYPOD_SCENE_MOTION_REPLACE, 1024, 240, &end);
    assert(start.from_x == 0);
    assert(start.from_y == 0);
    assert(start.to_y == 240);
    assert(end.from_y == 0);
    assert(end.to_y == 0);
    assert(crazypod_scene_motion_duration_ms(
               CRAZYPOD_SCENE_MOTION_PUSH) == 340);
}

static bool test_music_input_handler(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    void *context)
{
    int *calls = context;

    assert(state->route == MUSIC_ROUTE_MENU);
    assert(event->raw == 42);
    ++*calls;
    return true;
}

static int activation_calls;
static int render_calls;

static bool test_music_activate_handler(
    const struct route_state *state)
{
    assert(state->route == MUSIC_ROUTE_MENU);
    ++activation_calls;
    return true;
}

static void test_music_render_handler(
    const struct route_state *state)
{
    assert(state->route == MUSIC_ROUTE_MENU);
    ++render_calls;
}

static void test_feature_input_dispatcher(void)
{
    struct route_state state = { MUSIC_ROUTE_MENU, 0, -1 };
    const struct crazypod_input_event event = {
        42, 42, 0, false, false
    };
    int calls = 0;
    const struct crazypod_feature_bindings bindings = {
        .pressed = {
            [CRAZYPOD_FEATURE_MUSIC] =
                test_music_input_handler,
        },
        .activate = {
            [CRAZYPOD_FEATURE_MUSIC] =
                test_music_activate_handler,
        },
        .render = {
            [CRAZYPOD_FEATURE_MUSIC] =
                test_music_render_handler,
        },
        .context = &calls,
    };

    assert(!crazypod_feature_input_dispatch(
        &state, &event, CRAZYPOD_FEATURE_INPUT_RAW, &bindings));
    assert(crazypod_feature_input_dispatch(
        &state, &event, CRAZYPOD_FEATURE_INPUT_PRESSED, &bindings));
    assert(calls == 1);
    assert(crazypod_feature_activate_dispatch(&state, &bindings));
    assert(activation_calls == 1);
    assert(crazypod_feature_render_dispatch(&state, &bindings));
    assert(render_calls == 1);
    state.route = EXTRAS_ROUTE_MENU;
    assert(!crazypod_feature_input_dispatch(
        &state, &event, CRAZYPOD_FEATURE_INPUT_PRESSED, &bindings));
    assert(!crazypod_feature_activate_dispatch(&state, &bindings));
    assert(!crazypod_feature_render_dispatch(&state, &bindings));
}

int main(void)
{
    test_calendar();
    test_clock_format();
    test_bar_fill();
    test_panel_half_geometry();
    test_note_layout();
    test_editor();
    test_menu_layout();
    test_collation();
    test_alpha_jump_burst();
    test_alpha_jump_needs_sustained_spin();
    test_wheel_accel_slow_turn_stays_single();
    test_wheel_accel_fast_spin_multiplies();
    test_wheel_accel_a_full_spin_crosses_a_long_list();
    test_wheel_accel_a_slow_screen_does_not_accelerate();
    test_wheel_accel_pause_starts_from_rest();
    test_wheel_accel_turning_back_starts_from_rest();
    test_wheel_accel_respects_the_maximum();
    test_route_registry();
    test_navigation_commands();
    test_scene_motion();
    test_feature_input_dispatcher();
    return 0;
}
