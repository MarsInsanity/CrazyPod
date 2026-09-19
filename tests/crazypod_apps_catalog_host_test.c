/*
 * Which applications a panel offers, and what happens to a state file that
 * names one it does not have.
 *
 * The menu order is persisted by ID, and the set of IDs a build knows now
 * depends on the panel: the Mini has no Media app, no Game Boy and no Mini
 * Apps, because none of the three can be shown on 138x110 in four shades.
 * The risk is not that the list is short -- it is that a disk written by a
 * 6G puts an application the Mini cannot open into its menu, or that the
 * Mini's shorter list silently drops the 6G's ordering of what it does
 * have. Both are checked here.
 *
 * Compiled twice, once against each panel's configuration.
 */
#include "crazypod_apps.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) \
    do { \
        if(!(condition)) { \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while(0)

#ifdef HAVE_CRAZYPOD_MEDIA_LIBRARY
#define EXPECT_MEDIA true
#else
#define EXPECT_MEDIA false
#endif
#ifdef HAVE_CRAZYPOD_GAMEBOY
#define EXPECT_GAMEBOY true
#else
#define EXPECT_GAMEBOY false
#endif
#ifdef HAVE_CRAZYPOD_MINIAPPS
#define EXPECT_MINI_APPS true
#else
#define EXPECT_MINI_APPS false
#endif

static bool in_order(enum crazypod_app_id id)
{
    int index;

    for(index = 0; index < crazypod_apps_count(); ++index) {
        if(crazypod_apps_ordered_id(index) == id)
            return true;
    }
    return false;
}

static void test_catalog_matches_the_panel(void)
{
    crazypod_apps_reset();

    CHECK(crazypod_apps_count() == CRAZYPOD_APP_COUNT);
    CHECK(crazypod_apps_is_known(CRAZYPOD_APP_PHOTOS) == EXPECT_MEDIA);
    CHECK(crazypod_apps_is_known(CRAZYPOD_APP_GAMEBOY) == EXPECT_GAMEBOY);
    CHECK(crazypod_apps_is_known(CRAZYPOD_APP_MINI_APPS) ==
          EXPECT_MINI_APPS);
    CHECK(in_order(CRAZYPOD_APP_PHOTOS) == EXPECT_MEDIA);
    CHECK(in_order(CRAZYPOD_APP_GAMEBOY) == EXPECT_GAMEBOY);
    CHECK(in_order(CRAZYPOD_APP_MINI_APPS) == EXPECT_MINI_APPS);

    /* Music and Settings are on every panel, and neither can be hidden. */
    CHECK(crazypod_apps_is_known(CRAZYPOD_APP_MUSIC));
    CHECK(crazypod_apps_is_known(CRAZYPOD_APP_SETTINGS));
    CHECK(crazypod_apps_is_fixed(CRAZYPOD_APP_MUSIC));
    CHECK(crazypod_apps_is_fixed(CRAZYPOD_APP_SETTINGS));
    /* Nothing this build does not have may be pinned into the menu. */
    CHECK(crazypod_apps_is_fixed(CRAZYPOD_APP_GAMEBOY) == EXPECT_GAMEBOY);
}

static void test_restore_drops_what_this_panel_lacks(void)
{
    /* A menu order as a 6G would have written it: the full set, in an
     * arrangement the owner chose. */
    static const uint8_t saved[] = {
        CRAZYPOD_APP_NOTES,
        CRAZYPOD_APP_GAMEBOY,
        CRAZYPOD_APP_MUSIC,
        CRAZYPOD_APP_PHOTOS,
        CRAZYPOD_APP_BOOKS,
        CRAZYPOD_APP_MINI_APPS,
        CRAZYPOD_APP_SETTINGS,
    };
    int index;

    crazypod_apps_restore(saved, sizeof(saved) / sizeof(saved[0]),
                          0xffffffffu);

    CHECK(crazypod_apps_count() == CRAZYPOD_APP_COUNT);
    CHECK(in_order(CRAZYPOD_APP_PHOTOS) == EXPECT_MEDIA);
    CHECK(in_order(CRAZYPOD_APP_GAMEBOY) == EXPECT_GAMEBOY);
    CHECK(in_order(CRAZYPOD_APP_MINI_APPS) == EXPECT_MINI_APPS);

    /* The part of the arrangement this panel can honour survives: Notes
     * was first and still is, and Music still precedes Books. */
    CHECK(crazypod_apps_ordered_id(0) == CRAZYPOD_APP_NOTES);
    CHECK(crazypod_apps_order_index(CRAZYPOD_APP_MUSIC) <
          crazypod_apps_order_index(CRAZYPOD_APP_BOOKS));

    /* Every application appears exactly once, whatever the disk said. */
    for(index = 0; index < crazypod_apps_count(); ++index) {
        enum crazypod_app_id id = crazypod_apps_ordered_id(index);
        int other;
        int seen = 0;

        CHECK(crazypod_apps_is_known(id));
        for(other = 0; other < crazypod_apps_count(); ++other) {
            if(crazypod_apps_ordered_id(other) == id)
                ++seen;
        }
        CHECK(seen == 1);
    }
}

static void test_visible_and_hidden_add_up(void)
{
    crazypod_apps_reset();
    CHECK(crazypod_apps_visible_count() +
          crazypod_apps_hidden_count() == crazypod_apps_count());
    /* Hiding one moves it across; a fixed one cannot be hidden at all. */
    if(crazypod_apps_set_enabled(CRAZYPOD_APP_NOTES, false)) {
        CHECK(!crazypod_apps_is_enabled(CRAZYPOD_APP_NOTES));
        CHECK(crazypod_apps_visible_count() +
              crazypod_apps_hidden_count() == crazypod_apps_count());
    }
    CHECK(!crazypod_apps_set_enabled(CRAZYPOD_APP_MUSIC, false));
    CHECK(crazypod_apps_is_enabled(CRAZYPOD_APP_MUSIC));
}

int main(void)
{
    test_catalog_matches_the_panel();
    test_restore_drops_what_this_panel_lacks();
    test_visible_and_hidden_add_up();

    if(failures != 0) {
        printf("crazypod_apps_catalog_host_test: %d failure(s)\n",
               failures);
        return 1;
    }
    printf("crazypod_apps_catalog_host_test: %d applications on this "
           "panel, all checks passed\n", CRAZYPOD_APP_COUNT);
    return 0;
}
