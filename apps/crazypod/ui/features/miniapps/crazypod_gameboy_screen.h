#ifndef CRAZYPOD_GAMEBOY_SCREEN_H
#define CRAZYPOD_GAMEBOY_SCREEN_H

#include "../../../gameboy/crazypod_gameboy.h"

#ifdef HAVE_CRAZYPOD_GAMEBOY
enum crazypod_gameboy_result crazypod_gameboy_screen_run(int index);
const char *crazypod_gameboy_screen_error(
    enum crazypod_gameboy_result result);
#else
/* Nothing can reach the route that would call these: the application is
 * not in the catalog on a panel too small to show a Game Boy frame. */
static inline enum crazypod_gameboy_result
crazypod_gameboy_screen_run(int index)
{
    (void)index;
    return CRAZYPOD_GAMEBOY_BAD_ROM;
}

static inline const char *crazypod_gameboy_screen_error(
    enum crazypod_gameboy_result result)
{
    (void)result;
    return "";
}
#endif

#endif
