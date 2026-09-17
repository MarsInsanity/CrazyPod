#include <string.h>

#include "crazypod_wheel_accel.h"

/* Clicks per second, and what each band multiplies the base step by. */
static const struct {
    int rate;
    int multiplier;
} bands[] = {
    { 34, 6 },
    { 26, 4 },
    { 18, 3 },
    { 10, 2 },
    {  0, 1 },
};

void crazypod_wheel_accel_reset(
    struct crazypod_wheel_accel_state *state)
{
    if(state != NULL)
        memset(state, 0, sizeof(*state));
}

static int multiplier_for(int rate)
{
    unsigned i;

    for(i = 0; i < sizeof(bands) / sizeof(bands[0]); ++i)
        if(rate >= bands[i].rate)
            return bands[i].multiplier;
    return 1;
}

int crazypod_wheel_accel_step(
    struct crazypod_wheel_accel_state *state,
    int direction, int clicks, long now,
    long idle_ticks, long ticks_per_second, int maximum)
{
    int sign;
    int base;
    int step;
    long gap;
    int rate;

    if(clicks < 1)
        clicks = 1;
    if(maximum < 1)
        maximum = 1;
    if(state == NULL || direction == 0 || ticks_per_second <= 0)
        return clicks < maximum ? clicks : maximum;

    sign = direction > 0 ? 1 : -1;
    gap = now - state->last_tick;
    if(!state->valid || state->direction != sign ||
       gap < 0 || gap > idle_ticks) {
        /* A new spin starts from rest however fast the last one ended. */
        state->rate = 0;
        state->direction = sign;
        state->valid = true;
    }
    else {
        /*
         * Two events in the same tick would divide by zero, and they are
         * as fast as this can measure anyway: treat them as one tick.
         */
        if(gap < 1)
            gap = 1;
        rate = (int)((long)clicks * ticks_per_second / gap);
        /* Smooth, so one quick flick in a slow turn does not accelerate. */
        state->rate = (state->rate * 2 + rate) / 3;
    }
    state->last_tick = now;

    /*
     * The clicks are honoured whole. Discarding some of them because the
     * screen was busy would lose part of a turn the hand really made, and
     * the wheel would feel like it was ignoring you -- a worse fault than
     * the leap it would prevent. The leap is the slow screen's to answer
     * for, not the wheel's.
     */
    base = clicks;
    step = base * multiplier_for(state->rate);
    if(step < 1)
        step = 1;
    if(step > maximum)
        step = maximum;
    return step;
}
