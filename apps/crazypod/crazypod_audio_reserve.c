#include "config.h"

#ifdef HAVE_CRAZYPOD_UI

#include "buflib.h"
#include "kernel.h"
#include "core_alloc.h"

#include "crazypod_audio_memory_policy.h"
#include "crazypod_audio_reserve.h"
#include "crazypod_diag_log.h"

static int audio_reserve_handle;

bool crazypod_audio_reserve_acquire(void)
{
    if(audio_reserve_handle > 0)
        return true;
    audio_reserve_handle = core_alloc_ex(
        CRAZYPOD_AUDIO_BUFFER_FLOOR, &buflib_ops_locked);
    if(audio_reserve_handle > 0)
        return true;
    /*
     * The reserve is pinned, so it needs four contiguous megabytes and
     * buflib cannot shuffle a locked block out of the way to find them.
     * Yielding lets whatever else is mid-allocation finish and lets
     * buflib compact what it can, which is enough when the failure is a
     * transient rather than the arena genuinely being full.
     */
    for(int attempt = 0; attempt < 4 && audio_reserve_handle <= 0;
        ++attempt) {
        yield();
        audio_reserve_handle = core_alloc_ex(
            CRAZYPOD_AUDIO_BUFFER_FLOOR, &buflib_ops_locked);
    }
    if(audio_reserve_handle <= 0)
        crazypod_diag_log(
            "audio", "reserve of %lu bytes refused, largest free %lu",
            (unsigned long)CRAZYPOD_AUDIO_BUFFER_FLOOR,
            (unsigned long)core_allocatable());
    return audio_reserve_handle > 0;
}

void crazypod_audio_reserve_release(void)
{
    if(audio_reserve_handle > 0)
        audio_reserve_handle = core_free(audio_reserve_handle);
}

bool crazypod_audio_reserve_is_held(void)
{
    return audio_reserve_handle > 0;
}

#endif
