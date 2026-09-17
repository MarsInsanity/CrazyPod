#ifndef CRAZYPOD_WHEEL_ACCEL_H
#define CRAZYPOD_WHEEL_ACCEL_H

#include <stdbool.h>

/*
 * How far one wheel event moves a list.
 *
 * The driver reports how many clicks arrived since it was last asked, which
 * is both how far the wheel turned and how long the UI took to come back
 * for them -- a slow screen coalesces clicks and then the selection leaps,
 * which is what "I don't see the steps in between" describes. So the clicks
 * set a small base step, and the multiplier comes from angular speed:
 * clicks per second, which is what the hand is actually doing and does not
 * change when the screen is slow.
 *
 * A long list wants to be crossable, so a sustained fast spin multiplies;
 * a careful turn stays at one item per click, which is what picking a
 * single song out of three thousand needs.
 */

struct crazypod_wheel_accel_state {
    long last_tick;
    int direction;
    int rate;      /* smoothed clicks per second */
    bool valid;
};

void crazypod_wheel_accel_reset(
    struct crazypod_wheel_accel_state *state);

/*
 * clicks: what the driver reported for this event, at least 1.
 * now, idle_ticks: a gap longer than idle_ticks starts a new spin.
 * ticks_per_second: HZ.
 * maximum: the most this list will move in one event.
 */
int crazypod_wheel_accel_step(
    struct crazypod_wheel_accel_state *state,
    int direction, int clicks, long now,
    long idle_ticks, long ticks_per_second, int maximum);

#endif
