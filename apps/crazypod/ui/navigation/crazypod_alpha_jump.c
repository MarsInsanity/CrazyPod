#include <string.h>

#include "crazypod_alpha_jump.h"

void crazypod_alpha_jump_reset(
    struct crazypod_alpha_jump_state *state)
{
    if(state != NULL)
        memset(state, 0, sizeof(*state));
}

bool crazypod_alpha_jump_consume(
    struct crazypod_alpha_jump_state *state,
    enum crazypod_route route, int group,
    int direction, int steps, long now,
    long window_ticks, int threshold, int min_events)
{
    int sign;
    bool same_burst;

    if(state == NULL || direction == 0 ||
       steps <= 0 || window_ticks <= 0 || threshold <= 0)
        return false;
    if(min_events < 1)
        min_events = 1;
    sign = direction > 0 ? 1 : -1;
    same_burst =
        state->valid &&
        state->route == route &&
        state->group == group &&
        state->direction == sign &&
        (long)(now - state->last_tick) >= 0 &&
        (long)(now - state->last_tick) <= window_ticks;
    if(!same_burst) {
        state->route = route;
        state->group = group;
        state->direction = sign;
        state->steps = 0;
        state->events = 0;
        state->jumping = false;
        state->valid = true;
    }
    state->last_tick = now;
    if(state->steps < threshold) {
        state->steps += steps;
        if(state->steps > threshold)
            state->steps = threshold;
    }
    if(state->events < min_events)
        ++state->events;
    /*
     * Counting steps alone let one flick of the wheel jump a letter: a
     * single wheel event reports up to twelve steps, so the threshold was
     * reachable before the list had scrolled at all. Ask for a spin that is
     * both long and sustained across several events.
     */
    if(state->steps >= threshold && state->events >= min_events)
        state->jumping = true;
    return state->jumping;
}
