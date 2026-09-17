#ifndef CRAZYPOD_PANEL_GEOMETRY_H
#define CRAZYPOD_PANEL_GEOMETRY_H

#include <stdbool.h>

/*
 * Where the pieces of one half of a split glass panel go.
 *
 * The home capsule and the lock screen media widget are each drawn as two
 * halves so the top and the bottom can take the screen's own corner radii.
 * Each half is a clip the height of the half, holding a layer the height of
 * the whole panel, so the rounded corner it does not own falls outside the
 * clip and is never seen.
 *
 * The border is its own box because it has to reach past the split by at
 * least a pixel: at a corner radius of zero the layer ended exactly on the
 * split, so the top half drew its bottom border and the bottom half its top
 * border, one against the other, as a two-pixel line across the middle of
 * the widget.
 */
struct crazypod_panel_half {
    int clip_y;
    int clip_height;
    int layer_y;
    int layer_height;
    int glass_y;
    int border_y;
    int border_height;
};

void crazypod_panel_half_geometry(int panel_height, int radius, bool top,
                                  struct crazypod_panel_half *half);

#endif
