#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "buflib.h"
#include "crazypod_audio_memory_policy.h"

#define TEST_POOL_SIZE (8u * 1024u * 1024u)
#define LATE_ALLOCATION_COUNT 64u
#define LATE_ALLOCATION_SIZE (32u * 1024u)
#define OVERSIZED_ALLOCATION (3u * 1024u * 1024u)

static union buflib_data test_pool[
    TEST_POOL_SIZE / sizeof(union buflib_data)];
static int reserved_handles[CRAZYPOD_RUNTIME_HANDLE_HEADROOM];
static int late_handles[LATE_ALLOCATION_COUNT];
static bool playback_running;
static int shrink_requests;

void __assert(const char *file, int line, const char *expression)
{
    fprintf(stderr, "%s:%d: assertion failed: %s\n",
        file, line, expression);
    abort();
}

void panicf(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    abort();
}

static int audio_shrink(
    int handle, unsigned hints, void *start, size_t size)
{
    (void)handle;
    (void)hints;
    (void)start;
    (void)size;
    ++shrink_requests;
    return crazypod_audio_buffer_may_shrink(playback_running, size)
        ? BUFLIB_CB_OK : BUFLIB_CB_CANNOT_SHRINK;
}

int main(void)
{
    static struct buflib_callbacks audio_ops = {
        .move_callback = NULL,
        .shrink_callback = audio_shrink,
        .sync_callback = NULL,
    };
    struct buflib_context context;
    unsigned char *audio_data;
    size_t audio_size;
    size_t headroom;
    unsigned index;
    int headroom_handle;
    int audio_handle;
    int oversized_handle;

    buflib_init(&context, test_pool, sizeof(test_pool));
    for (index = 0; index < CRAZYPOD_RUNTIME_HANDLE_HEADROOM; ++index)
    {
        reserved_handles[index] = buflib_alloc(&context, 1);
        assert(reserved_handles[index] > 0);
    }
    headroom = crazypod_audio_runtime_headroom(
        buflib_allocatable(&context));
    assert(headroom > LATE_ALLOCATION_COUNT * LATE_ALLOCATION_SIZE);
    headroom_handle = buflib_alloc_ex(
        &context, headroom, &buflib_ops_locked);
    assert(headroom_handle > 0);
    audio_handle = buflib_alloc_maximum(
        &context, &audio_size, &audio_ops);
    assert(audio_handle > 0);
    assert(audio_size >= 3u * 1024u * 1024u);
    buflib_free(&context, headroom_handle);
    while (index > 0)
        buflib_free(&context, reserved_handles[--index]);

    audio_data = buflib_get_data(&context, audio_handle);
    audio_data[0] = 0x5a;
    playback_running = true;
    for (index = 0; index < LATE_ALLOCATION_COUNT; ++index)
    {
        late_handles[index] = buflib_alloc(
            &context, LATE_ALLOCATION_SIZE);
        assert(late_handles[index] > 0);
    }
    assert(shrink_requests == 0);

    oversized_handle = buflib_alloc(
        &context, OVERSIZED_ALLOCATION);
    assert(oversized_handle < 0);
    assert(shrink_requests > 0);
    assert(buflib_get_data(&context, audio_handle) == audio_data);
    assert(audio_data[0] == 0x5a);

    /*
     * A healthy arena walks end to end, and a stray write into a block
     * header is found by the walk rather than by whatever allocation next
     * happens to step through it.
     */
    {
        void *bad = (void *)1;
        size_t blocks = buflib_audit(&context, &bad);
        long *header;

        assert(bad == NULL);
        assert(blocks > 0);

        /*
         * A stomped handle-table pointer leaves the block list walking
         * perfectly while buflib_get_data() returns an address that is
         * nowhere. That is the shape of the crash on the device, so the
         * audit has to catch it as well as a bad length.
         */
        {
            union buflib_data *first = (union buflib_data *)(void *)test_pool;
            union buflib_data saved = first[1];

            first[1].handle = (union buflib_data *)(uintptr_t)0x0c0c4608;
            bad = NULL;
            blocks = buflib_audit(&context, &bad);
            assert(bad == (void *)&first[1]);
            first[1] = saved;
            bad = (void *)1;
            (void)buflib_audit(&context, &bad);
            assert(bad == NULL);
        }

        /* Zero a length field, which is what the device reported. */
        header = (long *)(void *)test_pool;
        *header = 0;
        bad = NULL;
        blocks = buflib_audit(&context, &bad);
        assert(bad == (void *)header);
        assert(blocks == 0);
    }

    puts("crazypod audio buflib integration tests passed");
    return 0;
}
