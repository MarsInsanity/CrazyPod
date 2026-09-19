#ifndef CRAZYPOD_APPS_H
#define CRAZYPOD_APPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

/*
 * These values are persisted. Retired IDs stay reserved so an older state
 * file can never turn Camera or Voice Memos into a different application.
 */
enum crazypod_app_id {
    CRAZYPOD_APP_INVALID = 0,
    CRAZYPOD_APP_MUSIC = 1,
    CRAZYPOD_APP_PODCASTS = 2,
    CRAZYPOD_APP_MINI_APPS = 3,
    CRAZYPOD_APP_SHUFFLE = 4,
    CRAZYPOD_APP_LOCK = 5,
    CRAZYPOD_APP_CAMERA_RETIRED = 6,
    CRAZYPOD_APP_PHOTOS = 7,
    CRAZYPOD_APP_CUSTOMIZE = 8,
    CRAZYPOD_APP_WORKOUTS = 9,
    CRAZYPOD_APP_VOICE_MEMOS_RETIRED = 10,
    CRAZYPOD_APP_BOOKS = 11,
    CRAZYPOD_APP_NOTES = 12,
    CRAZYPOD_APP_EXTRAS = 13,
    CRAZYPOD_APP_SETTINGS = 14,
    CRAZYPOD_APP_CLOCK = 15,
    CRAZYPOD_APP_CONTACTS = 16,
    CRAZYPOD_APP_CALENDAR = 17,
    CRAZYPOD_APP_STOPWATCH = 18,
    CRAZYPOD_APP_GAMEBOY = 19,
};

/*
 * How many of the enumerated applications this target actually builds.
 * The IDs above are persisted and never change; what varies is which of
 * them appear in the catalog and the menu order. A saved order naming an
 * application this target does not have is dropped on restore, so a state
 * file moves between devices without carrying a dead entry onto the Mini
 * or losing the 6G's on the way back.
 */
#ifdef HAVE_CRAZYPOD_MINIAPPS
#define CRAZYPOD_APP_HAS_MINI_APPS 1
#else
#define CRAZYPOD_APP_HAS_MINI_APPS 0
#endif
#ifdef HAVE_CRAZYPOD_MEDIA_LIBRARY
#define CRAZYPOD_APP_HAS_MEDIA 1
#else
#define CRAZYPOD_APP_HAS_MEDIA 0
#endif
#ifdef HAVE_CRAZYPOD_GAMEBOY
#define CRAZYPOD_APP_HAS_GAMEBOY 1
#else
#define CRAZYPOD_APP_HAS_GAMEBOY 0
#endif
/* Fourteen unconditional entries, plus the three that depend on the panel.
 * The catalog and the default order are both declared with this length, so
 * a mistake here is a compile error rather than a short read. */
#define CRAZYPOD_APP_COUNT (14 + \
    CRAZYPOD_APP_HAS_MINI_APPS + \
    CRAZYPOD_APP_HAS_MEDIA + \
    CRAZYPOD_APP_HAS_GAMEBOY)

/*
 * The width of the persisted menu order, which is deliberately not the
 * count above. It is the number of application IDs the file format
 * reserves, so the state file has one layout and one size on every target
 * and a disk written by a 6G still reads on a Mini. What the Mini does not
 * build it drops on restore; put the disk back in a 6G and those
 * applications return in their default places rather than where they were.
 */
#define CRAZYPOD_APP_SLOT_COUNT 17
#define CRAZYPOD_APP_LEGACY_COUNT 16

void crazypod_apps_reset(void);
void crazypod_apps_restore(const uint8_t *order, size_t count,
                           uint32_t enabled_mask);
void crazypod_apps_export(uint8_t *order, size_t capacity,
                          uint32_t *enabled_mask);

int crazypod_apps_count(void);
enum crazypod_app_id crazypod_apps_ordered_id(int index);
int crazypod_apps_order_index(enum crazypod_app_id id);

int crazypod_apps_visible_count(void);
enum crazypod_app_id crazypod_apps_visible_id(int index);
int crazypod_apps_visible_index(enum crazypod_app_id id);

int crazypod_apps_hidden_count(void);
enum crazypod_app_id crazypod_apps_hidden_id(int index);

bool crazypod_apps_is_known(enum crazypod_app_id id);
bool crazypod_apps_is_fixed(enum crazypod_app_id id);
bool crazypod_apps_is_enabled(enum crazypod_app_id id);
bool crazypod_apps_set_enabled(enum crazypod_app_id id, bool enabled);
bool crazypod_apps_move(enum crazypod_app_id id, int direction);

#endif
