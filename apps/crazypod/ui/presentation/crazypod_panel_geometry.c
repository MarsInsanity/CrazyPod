#include <stddef.h>

#include "crazypod_panel_geometry.h"

void crazypod_panel_half_geometry(int panel_height, int radius, bool top,
                                  struct crazypod_panel_half *half)
{
    int split;
    int overlap;

    if(half == NULL)
        return;
    if(panel_height < 0)
        panel_height = 0;
    if(radius < 0)
        radius = 0;
    if(radius > panel_height / 2)
        radius = panel_height / 2;
    split = panel_height / 2;
    /* A square corner still needs a pixel of reach, or the border lands on
     * the split and the two halves draw a line down the middle. */
    overlap = radius > 0 ? radius : 1;

    half->clip_y = top ? 0 : split;
    half->clip_height = top ? split : panel_height - split;
    half->layer_y = top ? 0 : -radius;
    half->layer_height = half->clip_height + radius;
    half->glass_y = top ? 0 : radius - split;
    half->border_y = top ? 0 : -overlap;
    half->border_height = half->clip_height + overlap;
}
