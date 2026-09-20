#ifndef CRAZYPOD_UI_METRICS_H
#define CRAZYPOD_UI_METRICS_H

#include "config.h"

#include "lcd.h"

/*
 * Where the furniture goes.
 *
 * The product UI was drawn on a 320x240 canvas with a list down the left and
 * a preview beside it. The Mini's panel is 138x110 at about the same pixel
 * pitch: a little over a sixth of the area, and no room at all for two
 * columns. These are the numbers for both, kept together so the two layouts
 * can be read against each other rather than found one constant at a time.
 *
 * The compact column is not the full one scaled. A list row is as tall as
 * the type it holds plus somewhere to put it, and type stops scaling before
 * a screen does -- so the row heights come from the faces the compact ladder
 * resolves to, and everything else is fitted around them.
 */

#ifdef HAVE_CRAZYPOD_COMPACT_UI

/* Status bar: one line of 8px type with a battery beside it. */
#define CRAZYPOD_METRIC_STATUS_HEIGHT 12

/*
 * The list takes the whole width. There is no preview: a cover at this size
 * would be a smudge, and the room it wants is the room the titles need.
 */
#define CRAZYPOD_METRIC_MENU_PANEL_WIDTH LCD_WIDTH

#define CRAZYPOD_METRIC_MENU_HEADER_X 4
#define CRAZYPOD_METRIC_MENU_HEADER_Y 13
#define CRAZYPOD_METRIC_MENU_HEADER_WIDTH (LCD_WIDTH - 8)
#define CRAZYPOD_METRIC_MENU_HEADER_HEIGHT 12

/*
 * Six rows of fourteen fill what is left exactly. Fourteen is the line box
 * of the 9px face the row titles resolve to, which is the smallest size
 * that is still read rather than guessed at on this panel.
 */
#define CRAZYPOD_METRIC_MENU_ROWS 6
#define CRAZYPOD_METRIC_MENU_ROW_X 0
#define CRAZYPOD_METRIC_MENU_ROW_Y 26
#define CRAZYPOD_METRIC_MENU_ROW_WIDTH (LCD_WIDTH - 4)
#define CRAZYPOD_METRIC_MENU_ROW_HEIGHT 14
#define CRAZYPOD_METRIC_MENU_ROW_STEP 14
/* A rounded selection bar loses two pixels of its own height to the corner
 * at this size, and the corner is not visible anyway. */
#define CRAZYPOD_METRIC_MENU_ROW_RADIUS 0

#define CRAZYPOD_METRIC_MENU_SCROLL_X (LCD_WIDTH - 3)
#define CRAZYPOD_METRIC_MENU_SCROLL_Y 27
#define CRAZYPOD_METRIC_MENU_SCROLL_HEIGHT 82

/* The icon is drawn at its own size, with no disc behind it: a 21px disc
 * around a 14px glyph is most of a row here, and reads as a blob. */
#define CRAZYPOD_METRIC_ROW_ICON_DISC 0
#define CRAZYPOD_METRIC_ROW_ICON_X 2
#define CRAZYPOD_METRIC_ROW_ICON_SIZE 14
#define CRAZYPOD_METRIC_ROW_TEXT_X_WITH_ICON 18
#define CRAZYPOD_METRIC_ROW_TEXT_X 4
#define CRAZYPOD_METRIC_ROW_MARKER_X (CRAZYPOD_METRIC_MENU_ROW_WIDTH - 10)

/* The type the list is set in, as the size a 320x240 screen would ask for;
 * the compact ladder resolves these to 9px and 8px. */
#define CRAZYPOD_METRIC_ROW_TEXT_SIZE 12
#define CRAZYPOD_METRIC_HEADER_TEXT_SIZE 10

/*
 * The watch faces. On the large canvas the dial sits on the left of a wide
 * card with the time and date in a column beside it; there is no room for
 * two columns here, so the dial is centred and the readout goes under it.
 */
#define CRAZYPOD_METRIC_FACE_PANEL_X 2
#define CRAZYPOD_METRIC_FACE_PANEL_Y 14
#define CRAZYPOD_METRIC_FACE_PANEL_WIDTH (LCD_WIDTH - 4)
#define CRAZYPOD_METRIC_FACE_PANEL_HEIGHT (LCD_HEIGHT - 16)
#define CRAZYPOD_METRIC_FACE_PANEL_RADIUS 4
#define CRAZYPOD_METRIC_FACE_DIAL_SIZE 56
#define CRAZYPOD_METRIC_FACE_DIAL_X \
    ((CRAZYPOD_METRIC_FACE_PANEL_WIDTH - CRAZYPOD_METRIC_FACE_DIAL_SIZE) / 2)
#define CRAZYPOD_METRIC_FACE_DIAL_Y 3
/* The column under the dial: caption, the time itself, then the date. */
#define CRAZYPOD_METRIC_FACE_TEXT_X 0
#define CRAZYPOD_METRIC_FACE_TEXT_WIDTH CRAZYPOD_METRIC_FACE_PANEL_WIDTH
#define CRAZYPOD_METRIC_FACE_TIME_Y 62
#define CRAZYPOD_METRIC_FACE_TIME_SIZE 16
#define CRAZYPOD_METRIC_FACE_DETAIL_Y 80
#define CRAZYPOD_METRIC_FACE_DETAIL_SIZE 10

#else /* the 320x240 canvas the product UI was drawn on */

#define CRAZYPOD_METRIC_STATUS_HEIGHT 32

#define CRAZYPOD_METRIC_MENU_PANEL_WIDTH 160

#define CRAZYPOD_METRIC_MENU_HEADER_X 16
#define CRAZYPOD_METRIC_MENU_HEADER_Y 38
#define CRAZYPOD_METRIC_MENU_HEADER_WIDTH 128
#define CRAZYPOD_METRIC_MENU_HEADER_HEIGHT 24

#define CRAZYPOD_METRIC_MENU_ROWS 6
#define CRAZYPOD_METRIC_MENU_ROW_X 8
#define CRAZYPOD_METRIC_MENU_ROW_Y 64
#define CRAZYPOD_METRIC_MENU_ROW_WIDTH 140
#define CRAZYPOD_METRIC_MENU_ROW_HEIGHT 28
#define CRAZYPOD_METRIC_MENU_ROW_STEP 28
#define CRAZYPOD_METRIC_MENU_ROW_RADIUS 8

#define CRAZYPOD_METRIC_MENU_SCROLL_X 153
#define CRAZYPOD_METRIC_MENU_SCROLL_Y 66
#define CRAZYPOD_METRIC_MENU_SCROLL_HEIGHT 164

#define CRAZYPOD_METRIC_ROW_ICON_DISC 1
#define CRAZYPOD_METRIC_ROW_ICON_X 6
#define CRAZYPOD_METRIC_ROW_ICON_SIZE 21
#define CRAZYPOD_METRIC_ROW_TEXT_X_WITH_ICON 34
#define CRAZYPOD_METRIC_ROW_TEXT_X 12
#define CRAZYPOD_METRIC_ROW_MARKER_X 128

#define CRAZYPOD_METRIC_ROW_TEXT_SIZE 18
#define CRAZYPOD_METRIC_HEADER_TEXT_SIZE 15

#endif

#define CRAZYPOD_METRIC_MENU_PANEL_Y CRAZYPOD_METRIC_STATUS_HEIGHT
#define CRAZYPOD_METRIC_MENU_PANEL_HEIGHT \
    (LCD_HEIGHT - CRAZYPOD_METRIC_MENU_PANEL_Y)

/* What is left beside the list, if anything. Zero means there is no room
 * for a preview and the screens that would draw one must not. */
#define CRAZYPOD_METRIC_PREVIEW_X CRAZYPOD_METRIC_MENU_PANEL_WIDTH
#define CRAZYPOD_METRIC_PREVIEW_WIDTH \
    (LCD_WIDTH - CRAZYPOD_METRIC_MENU_PANEL_WIDTH)

#endif
