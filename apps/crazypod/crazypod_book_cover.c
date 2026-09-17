#include "config.h"

#ifdef HAVE_CRAZYPOD_UI

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bmp.h"
#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "jpeg_load.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "crazypod_book_cover.h"
#include "crazypod_books.h"
#include "crazypod_diag_log.h"
#include "crazypod_image.h"

#define BOOK_COVER_CACHE_DIRECTORY "/.crazypod/cache/books"
#define BOOK_COVER_CACHE_MAGIC 0x31564342u   /* "BCV1" */
#define BOOK_COVER_SLOTS 4
#define BOOK_COVER_WIDTH 72
#define BOOK_COVER_HEIGHT 101
#define BOOK_COVER_DECODE_EXTRA (64 * 1024)

/*
 * A decoded cover kept on the card.
 *
 * The four slots above are this boot's; this is every boot's. A cover the
 * firmware draws at 72x101 may be 1600x1600 in the file, and scaling it
 * down is the slowest thing the Books preview does -- so do it once, for
 * good, rather than once for every time the device is switched on. The
 * key already covers the file's size and date, so a replaced cover misses
 * and is decoded again.
 */
struct book_cover_cache_header {
    uint32_t magic;
    uint32_t key;
    uint16_t width;
    uint16_t height;
};

struct book_cover_slot {
    uint32_t key;
    bool valid;
    lv_image_dsc_t descriptor;
    fb_data pixels[BOOK_COVER_WIDTH * BOOK_COVER_HEIGHT];
};

static struct book_cover_slot cover_slots[BOOK_COVER_SLOTS];
static int next_cover_slot;

static uint32_t path_hash(const char *path)
{
    uint32_t hash = 2166136261u;

    while(*path != '\0') {
        hash ^= (unsigned char)*path++;
        hash *= 16777619u;
    }
    return hash;
}

static int ascii_lower(int value)
{
    return value >= 'A' && value <= 'Z'
        ? value - 'A' + 'a' : value;
}

static bool extension_is(const char *path, const char *wanted)
{
    const char *dot = strrchr(path, '.');

    if(dot == NULL)
        return false;
    while(*dot != '\0' && *wanted != '\0') {
        if(ascii_lower((unsigned char)*dot) !=
           ascii_lower((unsigned char)*wanted))
            return false;
        ++dot;
        ++wanted;
    }
    return *dot == '\0' && *wanted == '\0';
}

static bool decode_cover(const char *path, struct book_cover_slot *slot,
                         int max_width, int max_height)
{
    struct bitmap bitmap;
    fb_data *decode_buffer;
    size_t pixel_bytes =
        BOOK_COVER_WIDTH * BOOK_COVER_HEIGHT * sizeof(fb_data);
    size_t decode_bytes = pixel_bytes + BOOK_COVER_DECODE_EXTRA;
    int decode_handle;
    int result;
    int row;

    if(!extension_is(path, ".jpg") &&
       !extension_is(path, ".jpeg") &&
       !extension_is(path, ".bmp"))
        return false;
    decode_handle = core_alloc_ex(
        decode_bytes, &buflib_ops_locked);
    if(decode_handle < 0)
        return false;
    decode_buffer = core_get_data(decode_handle);
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = max_width;
    bitmap.height = max_height;
    bitmap.data = (unsigned char *)decode_buffer;

    crazypod_image_decode_lock();
    if(extension_is(path, ".jpg") ||
       extension_is(path, ".jpeg"))
        result = read_jpeg_file(
            path, &bitmap, decode_bytes,
            FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
            &format_native);
    else
        result = read_bmp_file(
            path, &bitmap, decode_bytes,
            FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
            &format_native);
    crazypod_image_decode_unlock();

    if(result < 0 || bitmap.width <= 0 || bitmap.height <= 0 ||
       bitmap.width > BOOK_COVER_WIDTH ||
       bitmap.height > BOOK_COVER_HEIGHT ||
       bitmap.data == NULL) {
        core_free(decode_handle);
        return false;
    }
    for(row = 0; row < bitmap.height; ++row) {
        memcpy(slot->pixels + row * bitmap.width,
               (fb_data *)bitmap.data + row * bitmap.width,
               (size_t)bitmap.width * sizeof(fb_data));
    }
    core_free(decode_handle);
    return crazypod_image_configure_rgb565(
        &slot->descriptor, slot->pixels,
        bitmap.width, bitmap.height);
}

/*
 * Say what one decode cost, in milliseconds, but only when it was slow
 * enough to be the freeze that was reported: a cover that lands inside a
 * frame is not worth a line on the card. The probe that runs before it is
 * timed where it lives, because it has callers of its own.
 */
#define BOOK_COVER_SLOW_TICKS (HZ / 4)

static void report_cost(int book_index, long decode_ticks, bool decoded)
{
    if(decode_ticks < BOOK_COVER_SLOW_TICKS)
        return;
    crazypod_diag_log("bookcover", "i=%d decode=%ldms%s", book_index,
                      decode_ticks * 1000 / HZ, decoded ? "" : " failed");
}

static void cache_path(char *buffer, size_t size, uint32_t key)
{
    snprintf(buffer, size, "%s/%08lx.cvr",
             BOOK_COVER_CACHE_DIRECTORY, (unsigned long)key);
}

static bool cache_load(uint32_t key, struct book_cover_slot *slot)
{
    struct book_cover_cache_header header;
    char path[MAX_PATH];
    size_t pixels;
    int fd;
    bool ok = false;

    cache_path(path, sizeof(path), key);
    fd = open(path, O_RDONLY);
    if(fd < 0)
        return false;
    if(read(fd, &header, sizeof(header)) == (ssize_t)sizeof(header) &&
       header.magic == BOOK_COVER_CACHE_MAGIC &&
       header.key == key &&
       header.width > 0 && header.width <= BOOK_COVER_WIDTH &&
       header.height > 0 && header.height <= BOOK_COVER_HEIGHT) {
        pixels = (size_t)header.width * header.height * sizeof(fb_data);
        ok = read(fd, slot->pixels, pixels) == (ssize_t)pixels &&
            crazypod_image_configure_rgb565(
                &slot->descriptor, slot->pixels,
                header.width, header.height);
    }
    close(fd);
    if(!ok)
        remove(path);
    return ok;
}

static void cache_store(uint32_t key, const struct book_cover_slot *slot,
                        int width, int height)
{
    struct book_cover_cache_header header;
    char path[MAX_PATH];
    char temporary[MAX_PATH];
    size_t pixels = (size_t)width * height * sizeof(fb_data);
    int fd;
    bool ok;

    if(width <= 0 || height <= 0)
        return;
    mkdir("/.crazypod");
    mkdir("/.crazypod/cache");
    mkdir(BOOK_COVER_CACHE_DIRECTORY);
    cache_path(path, sizeof(path), key);
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return;
    header.magic = BOOK_COVER_CACHE_MAGIC;
    header.key = key;
    header.width = (uint16_t)width;
    header.height = (uint16_t)height;
    ok = write(fd, &header, sizeof(header)) == (ssize_t)sizeof(header) &&
        write(fd, slot->pixels, pixels) == (ssize_t)pixels;
    close(fd);
    /* Renamed into place, so a cover half-written when the battery goes
     * is never read back as a cover. */
    if(!ok || rename(temporary, path) < 0)
        remove(temporary);
}

const lv_image_dsc_t *crazypod_book_cover_get(
    int book_index, int max_width, int max_height)
{
    const struct crazypod_book *book;
    uint32_t key;
    struct book_cover_slot *slot;
    long decode_start;
    int i;

    if(max_width <= 0 || max_width > BOOK_COVER_WIDTH ||
       max_height <= 0 || max_height > BOOK_COVER_HEIGHT)
        return NULL;
    if(!crazypod_book_probe(book_index))
        return NULL;
    book = crazypod_book_get(book_index);
    if(book == NULL || book->cover_path[0] == '\0')
        return NULL;

    key = path_hash(book->cover_path) ^
        book->size ^ (book->mtime * 16777619u) ^
        ((uint32_t)max_width << 16) ^ (uint32_t)max_height;
    for(i = 0; i < BOOK_COVER_SLOTS; ++i) {
        if(cover_slots[i].valid && cover_slots[i].key == key)
            return &cover_slots[i].descriptor;
    }

    slot = &cover_slots[next_cover_slot];
    next_cover_slot =
        (next_cover_slot + 1) % BOOK_COVER_SLOTS;
    if(slot->valid)
        lv_image_cache_drop(&slot->descriptor);
    slot->valid = false;
    if(cache_load(key, slot)) {
        slot->key = key;
        slot->valid = true;
        return &slot->descriptor;
    }
    decode_start = current_tick;
    if(!decode_cover(
           book->cover_path, slot, max_width, max_height)) {
        report_cost(book_index, current_tick - decode_start, false);
        return NULL;
    }
    report_cost(book_index, current_tick - decode_start, true);
    cache_store(key, slot, slot->descriptor.header.w,
                slot->descriptor.header.h);
    slot->key = key;
    slot->valid = true;
    return &slot->descriptor;
}

void crazypod_book_cover_reset(void)
{
    int i;

    for(i = 0; i < BOOK_COVER_SLOTS; ++i) {
        if(cover_slots[i].valid)
            lv_image_cache_drop(&cover_slots[i].descriptor);
    }
    memset(cover_slots, 0, sizeof(cover_slots));
    next_cover_slot = 0;
}

#endif
