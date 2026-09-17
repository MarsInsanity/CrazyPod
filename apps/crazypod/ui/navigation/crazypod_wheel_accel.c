#include <string.h>

#include "crazypod_wheel_accel.h"

/*
 * Clicks per second, and what each band multiplies the base step by.
 *
 * The bottom of the table is what a careful turn feels like and has not
 * moved. The top is for the spin you make to cross four thousand songs,
 * and it was far too gentle: six times a four-click event, against a
 * ceiling of twelve, came to about a hundred and twenty songs a second --
 * half a minute from A to Z. The curve now keeps climbing past the point
 * an ordinary turn reaches, so the hardest spin the wheel can take is
 * worth spinning.
 */
static const struct {
    int rate;
    int multiplier;
} bands[] = {
    { 40, 16 },
    { 32, 11 },
    { 26, 7 },
    { 18, 4 },
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
        state->rate_scaled = 0;
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
        /*
         * Smooth, so one quick flick in a slow turn does not accelerate.
         *
         * Scaled by eight, and rounded on the way out. Averaged in whole
         * clicks per second, a two-thirds average never arrives at the rate
         * it is averaging -- truncation stops it two or three short, and
         * two or three short is exactly enough to put the top of the curve
         * out of reach of any spin a hand can make.
         */
        state->rate_scaled =
            (state->rate_scaled * 2 + rate * 8) / 3;
        state->rate = (state->rate_scaled + 4) / 8;
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
