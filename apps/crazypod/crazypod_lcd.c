#include "config.h"

#include "crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "system.h"
#include "version.h"

#include "crazypod_boot_logo.h"
#include "lvgl.h"

#include "crazypod_mono.h"
#include "crazypod_pixel.h"
#include "crazypod_lcd.h"

/*
 * The panel's own framebuffer, in the panel's own format: one RGB565 pixel
 * per element on the colour iPods, four 2bpp pixels per element on the Mini.
 * Rockbox's update path indexes it by element rather than by pixel, so both
 * shapes are addressed the same way.
 */
#ifdef HAVE_CRAZYPOD_MONO_UI
/*
 * A 138-pixel row is 34.5 elements, and the Mini's update path rounds its
 * window out to whole 8-pixel groups and can start two pixels to the left of
 * the screen when the display is flipped. Both overshoot the framebuffer by
 * one element at each end, so the rows sit inside a buffer with a spare row
 * on either side rather than at its edges.
 */
#define FRAMEBUFFER_GUARD_ROWS 1
#else
#define FRAMEBUFFER_GUARD_ROWS 0
#endif

#define FRAMEBUFFER_ROW_BYTES (LCD_FBWIDTH * sizeof(fb_data))
#define FRAMEBUFFER_BYTES (LCD_FBHEIGHT * FRAMEBUFFER_ROW_BYTES)

static fb_data crazypod_framebuffer_storage
    [LCD_FBHEIGHT + 2 * FRAMEBUFFER_GUARD_ROWS][LCD_FBWIDTH]
    IRAM_LCDFRAMEBUFFER CACHEALIGN_AT_LEAST_ATTR(16);

#define crazypod_framebuffer \
    (crazypod_framebuffer_storage + FRAMEBUFFER_GUARD_ROWS)

static lv_image_dsc_t boot_logo_descriptor;

static void *framebuffer_address(int x, int y)
{
    return &crazypod_framebuffer[y][x];
}

struct frame_buffer_t lcd_framebuffer_default = {
    .fb_ptr = &crazypod_framebuffer_storage[FRAMEBUFFER_GUARD_ROWS][0],
    .get_address_fn = framebuffer_address,
    .stride = LCD_WIDTH,
    .elems = LCD_FBWIDTH * LCD_FBHEIGHT,
};

/*
 * What the composition helpers below paint with. On a colour panel that is
 * simply the pixel; on the Mini it is a brightness level, because a single
 * pixel is not separately addressable there.
 */
#ifdef HAVE_CRAZYPOD_MONO_UI
typedef unsigned crazypod_ink_t;

static crazypod_ink_t ink_rgb(unsigned red, unsigned green, unsigned blue)
{
    return crazypod_mono_level_from_luma(
        (red * 77u + green * 150u + blue * 29u) >> 8);
}

static void fb_fill(int x, int y, int width, int height, crazypod_ink_t ink)
{
    unsigned value = crazypod_mono_hardware_value(ink);
    int row;

    if(x < 0) {
        width += x;
        x = 0;
    }
    if(y < 0) {
        height += y;
        y = 0;
    }
    if(x + width > LCD_WIDTH)
        width = LCD_WIDTH - x;
    if(y + height > LCD_HEIGHT)
        height = LCD_HEIGHT - y;
    for(row = 0; row < height; ++row)
        crazypod_mono_fill_row(
            &crazypod_framebuffer[y + row][0], x, width, value);
}

static void fb_set(int x, int y, crazypod_ink_t ink)
{
    if(x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT)
        return;
    crazypod_mono_fill_row(&crazypod_framebuffer[y][0], x, 1,
                           crazypod_mono_hardware_value(ink));
}
#else
typedef fb_data crazypod_ink_t;

static crazypod_ink_t ink_rgb(unsigned red, unsigned green, unsigned blue)
{
    return LCD_RGBPACK(red, green, blue);
}

static void fb_fill(int x, int y, int width, int height, crazypod_ink_t ink)
{
    int row;

    if(x < 0) {
        width += x;
        x = 0;
    }
    if(y < 0) {
        height += y;
        y = 0;
    }
    if(x + width > LCD_WIDTH)
        width = LCD_WIDTH - x;
    if(y + height > LCD_HEIGHT)
        height = LCD_HEIGHT - y;
    for(row = 0; row < height; ++row) {
        fb_data *pixel = &crazypod_framebuffer[y + row][x];
        fb_data *end = pixel + width;

        while(pixel < end)
            *pixel++ = ink;
    }
}

static void fb_set(int x, int y, crazypod_ink_t ink)
{
    if(x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT)
        return;
    crazypod_framebuffer[y][x] = ink;
}
#endif

static struct viewport crazypod_viewport = {
    .x = 0,
    .y = 0,
    .width = LCD_WIDTH,
    .height = LCD_HEIGHT,
    .flags = 0,
    .font = 0,
    .drawmode = DRMODE_SOLID,
    .buffer = &lcd_framebuffer_default,
    .fg_pattern = LCD_WHITE,
    .bg_pattern = LCD_BLACK,
};

struct viewport *lcd_current_viewport = &crazypod_viewport;

void lcd_set_foreground(unsigned color)
{
    lcd_current_viewport->fg_pattern = color;
}

unsigned lcd_get_foreground(void)
{
    return lcd_current_viewport->fg_pattern;
}

void lcd_set_background(unsigned color)
{
    lcd_current_viewport->bg_pattern = color;
}

unsigned lcd_get_background(void)
{
    return lcd_current_viewport->bg_pattern;
}

void lcd_fillrect(int x, int y, int width, int height)
{
    int left = x;
    int top = y;
    int right = x + width;
    int bottom = y + height;

    if(left < lcd_current_viewport->x)
        left = lcd_current_viewport->x;
    if(top < lcd_current_viewport->y)
        top = lcd_current_viewport->y;
    if(right > lcd_current_viewport->x + lcd_current_viewport->width)
        right = lcd_current_viewport->x + lcd_current_viewport->width;
    if(bottom > lcd_current_viewport->y + lcd_current_viewport->height)
        bottom = lcd_current_viewport->y + lcd_current_viewport->height;
    if(left >= right || top >= bottom)
        return;

    fb_fill(left, top, right - left, bottom - top,
            (crazypod_ink_t)lcd_current_viewport->fg_pattern);
}

#ifdef SIMULATOR
static int clamp_video_component(int value)
{
    if(value < 0)
        return 0;
    if(value > 255)
        return 255;
    return value;
}

static bool blit_yuv_to_framebuffer(
    unsigned char * const source[3],
    int source_x, int source_y, int stride,
    int destination_x, int destination_y,
    int width, int height)
{
    int row;
    int column;

    if(destination_x < 0 || destination_y < 0 ||
       destination_x + width > LCD_WIDTH ||
       destination_y + height > LCD_HEIGHT)
        return false;

    width &= ~1;
    height &= ~1;
    for(row = 0; row < height; ++row) {
        int source_row = source_y + row;
        const uint8_t *luma =
            source[0] + source_row * stride + source_x;
        const uint8_t *chroma_u =
            source[1] + (source_row >> 1) * (stride >> 1) +
            (source_x >> 1);
        const uint8_t *chroma_v =
            source[2] + (source_row >> 1) * (stride >> 1) +
            (source_x >> 1);
        int output_y = destination_y + row;

        for(column = 0; column < width; ++column) {
            int y = (int)luma[column] - 16;
            int u = (int)chroma_u[column >> 1] - 128;
            int v = (int)chroma_v[column >> 1] - 128;
            int red;
            int green;
            int blue;

            if(y < 0)
                y = 0;
            red = (298 * y + 409 * v + 128) >> 8;
            green = (298 * y - 100 * u - 208 * v + 128) >> 8;
            blue = (298 * y + 516 * u + 128) >> 8;
            fb_set(destination_x + column, output_y,
                   ink_rgb(clamp_video_component(red),
                           clamp_video_component(green),
                           clamp_video_component(blue)));
        }
    }
    return true;
}

void lcd_blit_yuv(unsigned char * const source[3],
                  int source_x, int source_y, int stride,
                  int destination_x, int destination_y,
                  int width, int height)
{
    if(!blit_yuv_to_framebuffer(
           source, source_x, source_y, stride,
           destination_x, destination_y, width, height))
        return;
    lcd_update_rect(destination_x, destination_y, width, height);
}
#elif defined(HAVE_CRAZYPOD_MONO_UI)
static bool blit_yuv_to_framebuffer(
    unsigned char * const source[3],
    int source_x, int source_y, int stride,
    int destination_x, int destination_y,
    int width, int height)
{
    int row;

    (void)source[1];
    (void)source[2];
    if(width <= 0 || height <= 0 ||
       destination_x < 0 || destination_y < 0 ||
       destination_x + width > LCD_WIDTH ||
       destination_y + height > LCD_HEIGHT)
        return false;

    /*
     * Four shades cannot carry chroma, so the colour planes are dropped and
     * the luma plane is quantised directly. Video-range luma is 16..235;
     * stretching it to the full range first keeps the darkest and brightest
     * shades reachable.
     */
    for(row = 0; row < height; ++row) {
        const unsigned char *luma =
            source[0] + (size_t)(source_y + row) * stride + source_x;
        int column;

        for(column = 0; column < width; ++column) {
            int value = ((int)luma[column] - 16) * 255 / 219;

            if(value < 0)
                value = 0;
            if(value > 255)
                value = 255;
            fb_set(destination_x + column, destination_y + row,
                   crazypod_mono_level_from_luma((unsigned)value));
        }
    }
    return true;
}
#else
static uint16_t video_line_buffer[LCD_WIDTH * 2]
    CACHEALIGN_AT_LEAST_ATTR(16);

extern void lcd_write_yuv420_lines(
    unsigned char const * const source[3],
    uint16_t *output, int width, int stride);

static bool blit_yuv_to_framebuffer(
    unsigned char * const source[3],
    int source_x, int source_y, int stride,
    int destination_x, int destination_y,
    int width, int height)
{
    unsigned int offset;
    unsigned char const *yuv_source[3];
    int row;

    width &= ~1;
    height &= ~1;
    if(width <= 0 || height <= 0 ||
       destination_x < 0 || destination_y < 0 ||
       destination_x + width > LCD_WIDTH ||
       destination_y + height > LCD_HEIGHT)
        return false;

    offset = stride * source_y;
    yuv_source[0] = source[0] + offset + source_x;
    yuv_source[1] = source[1] + (offset >> 2) + (source_x >> 1);
    yuv_source[2] = source[2] + (yuv_source[1] - source[1]);

    for(row = 0; row < height; row += 2) {
        if(destination_x == 0 && width == LCD_WIDTH) {
            lcd_write_yuv420_lines(
                yuv_source,
                (uint16_t *)&crazypod_framebuffer[
                    destination_y + row][0],
                width, stride);
        }
        else {
            lcd_write_yuv420_lines(
                yuv_source, video_line_buffer, width, stride);
            memcpy(&crazypod_framebuffer[
                       destination_y + row][destination_x],
                   video_line_buffer, width * sizeof(uint16_t));
            memcpy(&crazypod_framebuffer[
                       destination_y + row + 1][destination_x],
                   video_line_buffer + width,
                   width * sizeof(uint16_t));
        }
        yuv_source[0] += stride << 1;
        yuv_source[1] += stride >> 1;
        yuv_source[2] += stride >> 1;
    }
    return true;
}
#endif

void lcd_puts(int x, int y, const unsigned char *string)
{
    (void)x;
    (void)y;
    (void)string;
}

void lcd_putsf(int x, int y, const unsigned char *format, ...)
{
    va_list arguments;
    (void)x;
    (void)y;
    va_start(arguments, format);
    va_end(arguments);
}

struct viewport *lcd_init_viewport(struct viewport *vp)
{
    if(vp == NULL)
        vp = &crazypod_viewport;

    vp->x = 0;
    vp->y = 0;
    vp->width = LCD_WIDTH;
    vp->height = LCD_HEIGHT;
    vp->buffer = &lcd_framebuffer_default;
    return vp;
}

struct viewport *lcd_set_viewport(struct viewport *vp)
{
    struct viewport *previous = lcd_current_viewport;

    lcd_current_viewport = vp == NULL ? &crazypod_viewport : vp;
    if(lcd_current_viewport->buffer == NULL)
        lcd_current_viewport->buffer = &lcd_framebuffer_default;
    return previous;
}

struct viewport *lcd_set_viewport_ex(struct viewport *vp, int flags)
{
    (void)flags;
    return lcd_set_viewport(vp);
}

void lcd_clear_display(void)
{
    memset(&crazypod_framebuffer[0][0], 0, FRAMEBUFFER_BYTES);
}

void lcd_init(void)
{
    lcd_current_viewport = &crazypod_viewport;
    lcd_clear_display();
    lcd_init_device();
}

static void fill_screen(crazypod_ink_t ink)
{
    fb_fill(0, 0, LCD_WIDTH, LCD_HEIGHT, ink);
}

void crazypod_lcd_show_boot_logo(void)
{
#ifdef HAVE_CRAZYPOD_MONO_UI
    /*
     * The mark is a grey ramp over black, and the Mini's page is paper, so
     * it is drawn the way the rest of the monochrome UI is drawn: inverted,
     * ink where the artwork is brightest.
     */
    const crazypod_pixel_t *logo = crazypod_boot_logo_pixels();
    int origin_x = (LCD_WIDTH - CRAZYPOD_BOOT_LOGO_WIDTH) / 2;
    int origin_y = (LCD_HEIGHT - CRAZYPOD_BOOT_LOGO_HEIGHT) / 2;
    int row;

    fill_screen(CRAZYPOD_MONO_LEVEL_PAPER);
    if(origin_x < 0 || origin_y < 0) {
        lcd_update();
        return;
    }
    for(row = 0; row < CRAZYPOD_BOOT_LOGO_HEIGHT; ++row) {
        int column;

        for(column = 0; column < CRAZYPOD_BOOT_LOGO_WIDTH; ++column) {
            unsigned luma = crazypod_pixel_luma(
                logo[row * CRAZYPOD_BOOT_LOGO_WIDTH + column]);

            fb_set(origin_x + column, origin_y + row,
                   crazypod_mono_level_from_luma(255u - luma));
        }
    }
#else
    fill_screen(LCD_BLACK);
    crazypod_boot_logo_draw(
        &crazypod_framebuffer[0][0],
        LCD_WIDTH, LCD_HEIGHT, LCD_WIDTH);
#endif
    lcd_update();
}

const lv_image_dsc_t *crazypod_lcd_boot_logo_image(void)
{
    if(boot_logo_descriptor.header.magic != LV_IMAGE_HEADER_MAGIC) {
        memset(&boot_logo_descriptor, 0, sizeof(boot_logo_descriptor));
        boot_logo_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
        boot_logo_descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
        boot_logo_descriptor.header.w = CRAZYPOD_BOOT_LOGO_WIDTH;
        boot_logo_descriptor.header.h = CRAZYPOD_BOOT_LOGO_HEIGHT;
        boot_logo_descriptor.header.stride =
            CRAZYPOD_BOOT_LOGO_WIDTH * sizeof(crazypod_pixel_t);
        boot_logo_descriptor.data_size =
            CRAZYPOD_BOOT_LOGO_WIDTH *
            CRAZYPOD_BOOT_LOGO_HEIGHT * sizeof(crazypod_pixel_t);
        boot_logo_descriptor.data =
            (const uint8_t *)crazypod_boot_logo_pixels();
    }
    return &boot_logo_descriptor;
}

static void draw_glyph(const lv_font_t *font, uint32_t codepoint,
                       int x, int line_y, crazypod_ink_t color)
{
    lv_font_glyph_dsc_t glyph;
    const uint8_t *bitmap;
    int glyph_x;
    int glyph_y;
    int row;
    int column;

    if(!lv_font_get_glyph_dsc(font, &glyph, codepoint, 0))
        return;
    if(glyph.box_w == 0 || glyph.box_h == 0)
        return;

    glyph.req_raw_bitmap = 1;
    bitmap = glyph.resolved_font->get_glyph_bitmap(&glyph, NULL);
    if(bitmap == NULL || glyph.format != LV_FONT_GLYPH_FORMAT_A4)
        return;

    glyph_x = x + glyph.ofs_x;
    glyph_y = line_y + (font->line_height - font->base_line)
              - glyph.box_h - glyph.ofs_y;

    for(row = 0; row < glyph.box_h; ++row) {
        for(column = 0; column < glyph.box_w; ++column) {
            unsigned pixel_index = (unsigned)row * glyph.box_w + column;
            uint8_t packed = bitmap[pixel_index >> 1];
            uint8_t alpha = (pixel_index & 1) ? (packed & 0x0f)
                                             : (packed >> 4);
            if(alpha >= 5)
                fb_set(glyph_x + column, glyph_y + row, color);
        }
    }
}

static uint32_t next_utf8(const char **text)
{
    const unsigned char *cursor = (const unsigned char *)*text;
    uint32_t codepoint;

    if(cursor[0] < 0x80) {
        *text += 1;
        return cursor[0];
    }
    if((cursor[0] & 0xe0) == 0xc0 && (cursor[1] & 0xc0) == 0x80) {
        codepoint = ((uint32_t)(cursor[0] & 0x1f) << 6) |
            (uint32_t)(cursor[1] & 0x3f);
        *text += 2;
        return codepoint;
    }
    if((cursor[0] & 0xf0) == 0xe0 &&
       (cursor[1] & 0xc0) == 0x80 && (cursor[2] & 0xc0) == 0x80) {
        codepoint = ((uint32_t)(cursor[0] & 0x0f) << 12) |
            ((uint32_t)(cursor[1] & 0x3f) << 6) |
            (uint32_t)(cursor[2] & 0x3f);
        *text += 3;
        return codepoint;
    }
    if((cursor[0] & 0xf8) == 0xf0 &&
       (cursor[1] & 0xc0) == 0x80 && (cursor[2] & 0xc0) == 0x80 &&
       (cursor[3] & 0xc0) == 0x80) {
        codepoint = ((uint32_t)(cursor[0] & 0x07) << 18) |
            ((uint32_t)(cursor[1] & 0x3f) << 12) |
            ((uint32_t)(cursor[2] & 0x3f) << 6) |
            (uint32_t)(cursor[3] & 0x3f);
        *text += 4;
        return codepoint;
    }
    *text += 1;
    return 0xfffd;
}

static bool contains_non_ascii(const char *text)
{
    while(text != NULL && *text != '\0') {
        if((unsigned char)*text++ >= 0x80)
            return true;
    }
    return false;
}

static const lv_font_t *lcd_localized_font(const lv_font_t *font,
                                            const char *text)
{
    if(!contains_non_ascii(text))
        return font;
    if(font == &lv_font_montserrat_8)
        return &lv_font_crazypod_i18n_8;
    if(font == &lv_font_montserrat_10)
        return &lv_font_crazypod_i18n_10;
    if(font == &lv_font_montserrat_12)
        return &lv_font_crazypod_i18n_12;
    if(font == &lv_font_montserrat_16 || font == &lv_font_montserrat_24)
        return &lv_font_source_han_sans_sc_16_cjk;
    return font;
}

static int draw_text(const lv_font_t *font, const char *text,
                     int x, int y, int maximum_x, crazypod_ink_t color);

static void show_message(const char *title, const char *message,
                         const char *footer, crazypod_ink_t background)
{
    const lv_font_t *title_font = &lv_font_montserrat_16;
    const lv_font_t *body_font = &lv_font_montserrat_12;
    const crazypod_ink_t foreground = ink_rgb(255, 255, 255);
    const char *cursor;
    int footer_y;
    int bottom;
    int x = 14;
    int y = 18;

    title = crazypod_l10n_text(title);
    message = crazypod_l10n_text(message);
    title_font = lcd_localized_font(title_font, title);
    body_font = lcd_localized_font(body_font, message);
    cursor = message;
    fill_screen(background);
    /*
     * The footer gets a reserved line of its own rather than trailing the
     * message. Appended, it was the eleventh line of a ten-line screen and
     * a report that wrapped once pushed it off entirely -- which is how the
     * first build stamp failed to appear on the one panic it was written
     * for.
     */
    footer_y = LCD_HEIGHT - 12 - body_font->line_height;
    bottom = footer != NULL ? footer_y : LCD_HEIGHT - 12;

    while(*title != '\0') {
        lv_font_glyph_dsc_t glyph;
        uint32_t codepoint = next_utf8(&title);

        if(lv_font_get_glyph_dsc(title_font, &glyph, codepoint, 0)) {
            draw_glyph(title_font, codepoint, x, y, foreground);
            x += glyph.adv_w;
        }
    }

    x = 14;
    y = 48;
    while(*cursor != '\0' && y + body_font->line_height < bottom) {
        const char *line_start = cursor;
        int line_width = x;

        while(*cursor != '\0' && *cursor != '\n') {
            lv_font_glyph_dsc_t glyph;
            const char *next = cursor;
            uint32_t codepoint = next_utf8(&next);

            if(!lv_font_get_glyph_dsc(body_font, &glyph, codepoint, 0)) {
                cursor = next;
                continue;
            }
            if(line_width + glyph.adv_w > LCD_WIDTH - 14)
                break;
            line_width += glyph.adv_w;
            cursor = next;
        }

        {
            const char *character = line_start;
            int draw_x = x;

            while(character < cursor) {
                lv_font_glyph_dsc_t glyph;
                uint32_t codepoint = next_utf8(&character);

                if(lv_font_get_glyph_dsc(body_font, &glyph, codepoint, 0)) {
                    draw_glyph(body_font, codepoint, draw_x, y, foreground);
                    draw_x += glyph.adv_w;
                }
            }
        }

        if(*cursor == '\n')
            ++cursor;
        y += body_font->line_height + 3;
    }

    if(footer != NULL)
        (void)draw_text(body_font, footer, 14, footer_y,
                        LCD_WIDTH - 14, foreground);
    lcd_update();
}

static int draw_text(const lv_font_t *font, const char *text,
                     int x, int y, int maximum_x, crazypod_ink_t color)
{
    text = crazypod_l10n_text(text);
    font = lcd_localized_font(font, text);
    while(text != NULL && *text != '\0' && x < maximum_x) {
        lv_font_glyph_dsc_t glyph;
        uint32_t codepoint = next_utf8(&text);

        if(!lv_font_get_glyph_dsc(font, &glyph, codepoint, 0))
            continue;
        if(x + glyph.adv_w > maximum_x)
            break;
        draw_glyph(font, codepoint, x, y, color);
        x += glyph.adv_w;
    }
    return x;
}

void crazypod_lcd_draw_text(const char *text, int x, int y,
                            int maximum_x, uint32_t color)
{
    draw_text(&lv_font_montserrat_12, text, x, y, maximum_x,
              ink_rgb((color >> 16) & 255,
                      (color >> 8) & 255, color & 255));
}

static void format_video_time(char *buffer, size_t size, uint32_t seconds)
{
    uint32_t hours = seconds / 3600u;
    uint32_t minutes = seconds / 60u % 60u;
    uint32_t remainder = seconds % 60u;

    if(hours > 0)
        snprintf(buffer, size, "%lu:%02lu:%02lu",
                 (unsigned long)hours, (unsigned long)minutes,
                 (unsigned long)remainder);
    else
        snprintf(buffer, size, "%lu:%02lu",
                 (unsigned long)minutes, (unsigned long)remainder);
}

static void compose_video_controls(
    const char *title, uint32_t elapsed_seconds,
    uint32_t duration_seconds, int volume,
    bool paused, const char *message)
{
    const int panel_height = 42;
    const int panel_y = LCD_HEIGHT - panel_height;
    const int margin = 8;
    const int track_width = LCD_WIDTH - 2 * margin;
    const crazypod_ink_t white = ink_rgb(255, 255, 255);
    const crazypod_ink_t muted = ink_rgb(154, 160, 168);
    const crazypod_ink_t accent = ink_rgb(52, 120, 246);
    char elapsed[20];
    char duration[20];
    char volume_text[20];
    int progress_width = 0;
    int status_width = LCD_WIDTH * 54 / 320;
    int time_width = LCD_WIDTH * 62 / 320;

    fb_fill(0, panel_y, LCD_WIDTH, panel_height, ink_rgb(0, 0, 0));
    fb_fill(0, panel_y, LCD_WIDTH, 1, ink_rgb(42, 46, 53));

    draw_text(&lv_font_montserrat_12,
              paused ? CP_TR("PAUSED") : CP_TR("PLAYING"),
              margin, panel_y + 3, margin + status_width,
              paused ? muted : accent);
    draw_text(&lv_font_montserrat_12,
              (message != NULL && message[0] != '\0') ? message : title,
              margin + status_width + margin, panel_y + 3,
              LCD_WIDTH - margin, white);

    fb_fill(margin, panel_y + 19, track_width, 3, ink_rgb(48, 52, 60));
    if(duration_seconds > 0) {
        progress_width = (int)((uint64_t)elapsed_seconds *
                               (uint64_t)track_width / duration_seconds);
        if(progress_width > track_width)
            progress_width = track_width;
    }
    fb_fill(margin, panel_y + 19, progress_width, 3, accent);

    format_video_time(elapsed, sizeof(elapsed), elapsed_seconds);
    format_video_time(duration, sizeof(duration), duration_seconds);
    snprintf(volume_text, sizeof(volume_text), CP_FMT("VOL %d"), volume);
    draw_text(&lv_font_montserrat_12, elapsed,
              margin, panel_y + 25, margin + time_width, muted);
    draw_text(&lv_font_montserrat_12, duration,
              margin + time_width + 2, panel_y + 25,
              margin + 2 * time_width + 2, muted);
    draw_text(&lv_font_montserrat_12,
              CP_TR("PLAY  -10s  +10s  MENU"),
              margin + 2 * time_width + 4, panel_y + 25,
              LCD_WIDTH - margin - time_width, muted);
    draw_text(&lv_font_montserrat_12, volume_text,
              LCD_WIDTH - margin - time_width + 2, panel_y + 25,
              LCD_WIDTH - 2, white);
}

void crazypod_lcd_draw_video_frame(
    unsigned char * const source[3],
    int source_x, int source_y, int stride,
    int destination_x, int destination_y,
    int width, int height,
    bool controls_visible, const char *title,
    uint32_t elapsed_seconds, uint32_t duration_seconds,
    int volume, bool paused, const char *message)
{
    memset(&crazypod_framebuffer[0][0], 0, FRAMEBUFFER_BYTES);
    if(!blit_yuv_to_framebuffer(
           source, source_x, source_y, stride,
           destination_x, destination_y, width, height))
        return;
    if(controls_visible)
        compose_video_controls(
            title, elapsed_seconds, duration_seconds,
            volume, paused, message);
    lcd_update();
}

void crazypod_lcd_show_panic(const char *message)
{
    /*
     * Say which build this is. A panic screen is often the only thing that
     * comes back from a device, and a report against a build nobody can
     * identify costs a round to sort out. It goes in the reserved footer
     * line: appended to the message it sat one line below the bottom of a
     * screen whose report had wrapped, and did not appear at all.
     */
    show_message(CP_TR("CRAZYPOD PANIC"), message, rbversion,
                 ink_rgb(132, 20, 35));
}

#endif
