#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include "lvgl.h"

#include "crazypod_ui_menu_layout.h"
#include "crazypod_ui_widgets.h"
#include "crazypod_empty_state.h"
#include "crazypod_menu_icon_assets.h"
#include "crazypod_menu_list.h"
#include "../navigation/crazypod_route_query.h"
#include "crazypod_marquee.h"
#include "../../crazypod_state.h"
#include "crazypod_menu_screen.h"
#include "../../crazypod_color.h"
#include "crazypod_ui_metrics.h"
#include "../../crazypod_runtime_font.h"

#define COLOR_WHITE 0xFFFFFF
#define CRAZYPOD_VISIBLE_ROWS CRAZYPOD_METRIC_MENU_ROWS

#ifdef HAVE_CRAZYPOD_MONO_UI
/*
 * The selection bar is the one place the monochrome build names its shades
 * directly instead of letting the design map choose them. Everywhere else
 * the map does the right thing on its own; here it cannot, because the bar
 * and the title on it are two separate colours in the design and both land
 * on ink. A black bar with the title knocked out of it in paper is the
 * highest contrast the panel has, and it is what an iPod list has always
 * looked like.
 */
#define ROW_SELECTED_FILL CRAZYPOD_MONO_INK
#define ROW_SELECTED_TEXT CRAZYPOD_MONO_PAPER
#define ROW_TEXT CRAZYPOD_MONO_INK
/*
 * Hierarchy is carried by shade, not by opacity. Fading ink towards the
 * page is how the design separates a heading from a title, and it works
 * because there are 256 steps between them; here there are four, and a
 * heading at a third opacity lands on the page it is printed on.
 */
#define HEADER_TEXT CRAZYPOD_MONO_SHADE_DARK
#define MARKER_TEXT CRAZYPOD_MONO_SHADE_DARK
#endif

static lv_obj_t *make_box(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_box(
        parent, x, y, width, height, radius, color, opacity);
}

static lv_obj_t *make_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_label(
        parent, text, font, color, opacity);
}

void crazypod_menu_screen_render(
    const struct route_state *state,
    const struct crazypod_menu_screen_context *context)
{
    int count = context->item_count;
    int start;
    int row;
    lv_obj_t *header;

    crazypod_menu_list_reset(state->route);
#ifdef HAVE_CRAZYPOD_MONO_UI
    header = crazypod_ui_widget_label_shade(
        context->parent, crazypod_route_query_title(state),
        crazypod_runtime_font_at_size(CRAZYPOD_METRIC_HEADER_TEXT_SIZE),
        HEADER_TEXT, LV_OPA_COVER);
#else
    header = make_label(context->parent, crazypod_route_query_title(state),
                        crazypod_runtime_font_at_size(
                            CRAZYPOD_METRIC_HEADER_TEXT_SIZE),
                        COLOR_WHITE, 85);
#endif
    lv_obj_set_pos(header, CRAZYPOD_METRIC_MENU_HEADER_X,
                   CRAZYPOD_METRIC_MENU_HEADER_Y);
    lv_obj_set_width(header, CRAZYPOD_METRIC_MENU_HEADER_WIDTH);
    lv_obj_set_height(header, CRAZYPOD_METRIC_MENU_HEADER_HEIGHT);
    lv_label_set_long_mode(header, LV_LABEL_LONG_MODE_DOTS);

    if(count <= 0) {
        if(state->route == DIY_ROUTE_WALLPAPER_FILES)
            crazypod_empty_state_render(
                context->parent, LV_SYMBOL_IMAGE, CP_TR("No Pictures"),
                CP_TR("Add JPG or BMP files to /Pictures."));
        else if(state->route == PHOTOS_ROUTE_LIBRARY)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_IMAGE,
                CP_TR("No Pictures"),
                CP_TR("Add JPG, JPEG or BMP files to /Pictures."));
        else if(state->route == PHOTOS_ROUTE_VIDEOS ||
                state->route == PHOTOS_ROUTE_DELETE_VIDEOS)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_PLAY,
                CP_TR("No Videos"),
                CP_TR("Convert MPG or MPEG files into /Videos."));
        else if(state->route == PHOTOS_ROUTE_FAVORITES)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_IMAGE,
                CP_TR("No Favorites"),
                               CP_TR("Hold Select on a photo to save it here."));
        else if(state->route == EXTRAS_ROUTE_MENU)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_DIRECTORY,
                CP_TR("Nothing Hidden"),
                CP_TR("Hide apps in Settings > Main Menu."));
        else if(state->route == UTILITIES_ROUTE_MENU)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_FILE,
                CP_TR("No Mini Apps"),
                CP_TR("Copy a CPK directly to /MiniApps."));
        else if(state->route == NOTES_ROUTE_DELETED)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_EDIT,
                CP_TR("Deleted Is Empty"),
                CP_TR("Deleted notes can be restored from here."));
        else if(state->route == BOOKS_ROUTE_LIBRARY)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_FILE,
                CP_TR("No Books"),
                CP_TR("Add EPUB, TXT or Markdown files to /Books."));
        else if(state->route == BOOKS_ROUTE_RECENTS)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_FILE,
                CP_TR("No Recent Books"),
                CP_TR("Open a book to add it here."));
        else if(state->route == BOOKS_ROUTE_FAVORITES)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_FILE,
                CP_TR("No Favorites"),
                CP_TR("Favorite a book from Book Actions."));
        else if(state->route == BOOKS_ROUTE_BOOKMARKS)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_FILE,
                CP_TR("No Bookmark"),
                CP_TR("Press PLAY while reading to save this page."));
        else if(state->route == PODCASTS_ROUTE_MENU)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_AUDIO,
                CP_TR("No Podcasts"),
                CP_TR("Add audio files under /Podcasts and rescan."));
        else if(state->route == CONTACTS_ROUTE_LIST)
            crazypod_empty_state_render(context->parent,
                NULL,
                CP_TR("No Contacts"),
                CP_TR("Add VCF files to /Contacts."));
        else if(state->route == WORKOUT_ROUTE_HISTORY)
            crazypod_empty_state_render(context->parent,
                LV_SYMBOL_PLAY,
                CP_TR("No Workouts"),
                CP_TR("Start a time-only workout first."));
        else
            crazypod_empty_state_render(
                context->parent, LV_SYMBOL_AUDIO, CP_TR("Nothing Here"),
                CP_TR("Add local music and rescan."));
        return;
    }

    start = crazypod_ui_menu_window_start(
        count, state->selected, CRAZYPOD_VISIBLE_ROWS);

    for(row = 0; row < CRAZYPOD_VISIBLE_ROWS; ++row) {
        int index = start + row;
        int y = CRAZYPOD_METRIC_MENU_ROW_Y +
                row * CRAZYPOD_METRIC_MENU_ROW_STEP;
        bool selected = index == state->selected;
        lv_obj_t *row_box;
        lv_obj_t *label;
        lv_obj_t *marker;
        const char *title;
        const lv_font_t *row_font = crazypod_runtime_font_at_size(
            CRAZYPOD_METRIC_ROW_TEXT_SIZE);
#ifdef HAVE_CRAZYPOD_MONO_UI
        uint32_t text_color = ROW_TEXT;
#else
        uint32_t text_color = COLOR_WHITE;
#endif
        int text_x = CRAZYPOD_METRIC_ROW_TEXT_X;
        int text_width = CRAZYPOD_METRIC_MENU_ROW_WIDTH - 2 * text_x;

        if(index >= count)
            break;
#ifdef HAVE_CRAZYPOD_MONO_UI
        row_box = crazypod_ui_widget_box_shade(
            context->parent,
            CRAZYPOD_METRIC_MENU_ROW_X, y,
            CRAZYPOD_METRIC_MENU_ROW_WIDTH,
            CRAZYPOD_METRIC_MENU_ROW_HEIGHT,
            CRAZYPOD_METRIC_MENU_ROW_RADIUS,
            ROW_SELECTED_FILL,
            selected ? LV_OPA_COVER : LV_OPA_TRANSP);
        if(selected)
            text_color = ROW_SELECTED_TEXT;
#else
        row_box = make_box(context->parent,
                           CRAZYPOD_METRIC_MENU_ROW_X, y,
                           CRAZYPOD_METRIC_MENU_ROW_WIDTH,
                           CRAZYPOD_METRIC_MENU_ROW_HEIGHT,
                           CRAZYPOD_METRIC_MENU_ROW_RADIUS,
                           selected ? context->primary_color : context->panel_color,
                           selected ? 220 : LV_OPA_TRANSP);
#endif
#ifndef HAVE_CRAZYPOD_MONO_UI
        if(selected) {
            if(context->gradient_highlight &&
               !crazypod_state_reduce_effects()) {
                lv_obj_set_style_bg_grad_color(
                    row_box, crazypod_ui_color(context->secondary_color), 0);
                lv_obj_set_style_bg_grad_dir(row_box, LV_GRAD_DIR_HOR, 0);
            }
            lv_obj_set_style_border_width(row_box, 1, 0);
            lv_obj_set_style_border_color(row_box,
                                           crazypod_ui_color(COLOR_WHITE), 0);
            lv_obj_set_style_border_opa(row_box, 90, 0);
        }
#endif

        {
            enum crazypod_menu_icon icon_id =
                crazypod_route_query_item_icon(state, index);
            const lv_image_dsc_t *icon_asset =
                crazypod_menu_icon_asset(icon_id);

            if(icon_asset != NULL) {
                lv_obj_t *circle = make_box(
                    row_box, 0, 0,
                    CRAZYPOD_METRIC_ROW_ICON_SIZE,
                    CRAZYPOD_METRIC_ROW_ICON_SIZE,
                    LV_RADIUS_CIRCLE, COLOR_WHITE,
                    CRAZYPOD_METRIC_ROW_ICON_DISC
                        ? (selected ? 45
                            : crazypod_state_reduce_effects()
                                ? LV_OPA_TRANSP : 18)
                        : LV_OPA_TRANSP);
                lv_obj_t *icon = lv_image_create(circle);

                lv_obj_align(circle, LV_ALIGN_LEFT_MID,
                             CRAZYPOD_METRIC_ROW_ICON_X, 0);
                lv_image_set_src(icon, icon_asset);
#ifdef HAVE_CRAZYPOD_MONO_UI
                lv_obj_set_style_image_recolor(
                    icon, crazypod_ui_shade(text_color), 0);
#else
                lv_obj_set_style_image_recolor(
                    icon, crazypod_ui_color(COLOR_WHITE), 0);
#endif
                lv_obj_set_style_image_recolor_opa(
                    icon, LV_OPA_COVER, 0);
#ifdef HAVE_CRAZYPOD_MONO_UI
                lv_obj_set_style_opa(icon, LV_OPA_COVER, 0);
#else
                lv_obj_set_style_opa(
                    icon, selected ? 255 : 170, 0);
#endif
                lv_obj_center(icon);
                crazypod_menu_list_bind_icon(row, circle, icon);
                text_x = CRAZYPOD_METRIC_ROW_TEXT_X_WITH_ICON;
                text_width = CRAZYPOD_METRIC_ROW_MARKER_X - text_x - 2;
            }
        }
        title = context->item_title(state, index);
        if(title == NULL)
            title = "";
#ifdef HAVE_CRAZYPOD_MONO_UI
        label = crazypod_ui_widget_label_shade(
            row_box, title, row_font, text_color, LV_OPA_COVER);
#else
        label = make_label(row_box, title, row_font,
                           text_color, selected ? 255 : 195);
#endif
        lv_obj_set_width(label, text_width);
        crazypod_ui_widget_align_row_label(
            label, text_x, CRAZYPOD_UI_ROW_LABEL_TEXT);
        crazypod_marquee_configure(label, selected);

#ifdef HAVE_CRAZYPOD_MONO_UI
        marker = crazypod_ui_widget_label_shade(
            row_box,
            context->item_is_current(state, index)
                ? LV_SYMBOL_OK :
            selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET,
            &lv_font_montserrat_8,
            selected ? ROW_SELECTED_TEXT : MARKER_TEXT, LV_OPA_COVER);
#else
        marker = make_label(row_box,
                            context->item_is_current(state, index)
                                ? LV_SYMBOL_OK :
                            selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET,
                            &lv_font_montserrat_8,
                            COLOR_WHITE, selected ? 205 : 90);
#endif
        crazypod_ui_widget_align_row_label(
            marker, CRAZYPOD_METRIC_ROW_MARKER_X,
            CRAZYPOD_UI_ROW_LABEL_MARKER);
        crazypod_menu_list_bind_row(row, row_box, label, marker);
    }

    if(count > CRAZYPOD_VISIBLE_ROWS) {
        int track_height = CRAZYPOD_METRIC_MENU_SCROLL_HEIGHT;
        int thumb_height;
        int thumb_y;
        lv_obj_t *bar;
        crazypod_ui_menu_scroll_thumb(
            count, state->selected, CRAZYPOD_VISIBLE_ROWS,
            CRAZYPOD_METRIC_MENU_SCROLL_Y, track_height,
            track_height / 8, &thumb_y, &thumb_height);
        bar = make_box(context->parent, CRAZYPOD_METRIC_MENU_SCROLL_X,
                       CRAZYPOD_METRIC_MENU_SCROLL_Y, 2, track_height, 1,
                       COLOR_WHITE, 25);
        (void)bar;
        bar = make_box(context->parent, CRAZYPOD_METRIC_MENU_SCROLL_X,
                       thumb_y, 2, thumb_height, 1, COLOR_WHITE, 155);
        crazypod_menu_list_bind_scroll_thumb(bar);
    }

}



#endif
