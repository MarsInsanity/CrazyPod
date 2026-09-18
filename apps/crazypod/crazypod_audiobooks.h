#ifndef CRAZYPOD_AUDIOBOOKS_H
#define CRAZYPOD_AUDIOBOOKS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "file.h"

/*
 * Audiobooks live under /Audiobooks (.m4b, .m4a and .mp3) and as .m4b files
 * anywhere under /Books. They play through the ordinary music queue, so the
 * Now Playing screen, the home capsule and resume-on-boot all apply; what
 * this module adds is the catalog, Nero "chpl" chapter tables, and a saved
 * listening position per book that survives pausing, switching books and
 * power cycles.
 */

#define CRAZYPOD_AUDIOBOOKS_MAX 64
#define CRAZYPOD_AUDIOBOOK_TITLE_SIZE 96
#define CRAZYPOD_AUDIOBOOK_AUTHOR_SIZE 64
/* Without a chapter table the chapter keys move by this much. */
#define CRAZYPOD_AUDIOBOOK_SKIP_MS (30u * 1000u)

struct crazypod_audiobook {
    char path[MAX_PATH];
    char title[CRAZYPOD_AUDIOBOOK_TITLE_SIZE];
    char author[CRAZYPOD_AUDIOBOOK_AUTHOR_SIZE];
    uint32_t size;
    uint32_t mtime;
    uint32_t length_ms;
    uint32_t position_ms;
    /* The cover inside the file, for every surface that draws one. */
    uint32_t artwork_offset;
    uint32_t artwork_size;
    uint8_t artwork_type;
    bool artwork_embedded;
    /* Chapters the file declares, learned with the tags and kept with
     * them, so a preview never has to open the file to count them. */
    int chapter_count;
    bool details_loaded;
    bool favorite;
};

#include "crazypod_audiobook_chapters.h"

void crazypod_audiobooks_init(void);
void crazypod_audiobooks_scan(void);
bool crazypod_audiobooks_scan_needed(void);
void crazypod_audiobooks_invalidate_scan(void);
int crazypod_audiobooks_count(void);
const struct crazypod_audiobook *crazypod_audiobook_get(int index);
/* Loads title, author and length from the file's tags; cheap once done. */
bool crazypod_audiobook_probe(int index);
int crazypod_audiobooks_recent_index(void);
/* Recency sequence shared with text books (0 = never listened). */
uint32_t crazypod_audiobook_recent_sequence(int index);

int crazypod_audiobook_chapter_count(int index);
/*
 * The same count, but only if it is already known -- it never opens the
 * file. Anything drawing a preview wants this one: the loading version
 * parses the file's atom tree, and it was doing so from inside a render.
 */
int crazypod_audiobook_chapter_count_known(int index);
const struct crazypod_audiobook_chapter *crazypod_audiobook_chapter_get(
    int index, int chapter);
int crazypod_audiobook_chapter_at(int index, uint32_t position_ms);

/* Starts the book at its saved position, replacing the music queue. */
bool crazypod_audiobook_play(int index);
/* Index of the book the queue is playing, or -1. */
int crazypod_audiobooks_current_index(void);
struct crazypod_track;
/* Fills a track record from the playing book. False when none is playing. */
bool crazypod_audiobooks_describe_current(struct crazypod_track *track);
uint32_t crazypod_audiobooks_current_length_ms(void);
bool crazypod_audiobook_is_current(int index);
bool crazypod_audiobook_is_playing(int index);
uint32_t crazypod_audiobook_position_ms(int index);
/* Pauses a playing book, resumes a paused one, starts any other. */
bool crazypod_audiobook_toggle(int index);
bool crazypod_audiobook_seek_chapter(int index, int chapter);
bool crazypod_audiobook_skip_chapter(int index, int direction);
/* Call regularly from the UI loop; saves the position when playback pauses,
 * stops or moves to another file, and periodically while the disk is awake. */
void crazypod_audiobooks_tick(long now);
/* Write the listening position out now, for a power-off or reboot. */
void crazypod_audiobooks_flush(void);
/* Favorites, shared with the Books app's Favorites list. */
bool crazypod_audiobook_is_favorite(int index);
bool crazypod_audiobook_toggle_favorite(int index);
int crazypod_audiobooks_favorite_count(void);
int crazypod_audiobooks_favorite_at(int position);
/* Index of the audiobook at this path, or -1. */
int crazypod_audiobooks_find_path(const char *path);

/* Wall time the last chapter seek took to land, for the perf log. */
uint32_t crazypod_audiobooks_last_seek_ms(void);

#endif
