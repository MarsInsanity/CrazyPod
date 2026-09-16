#include "crazypod_diag_log.h"

#ifdef CRAZYPOD_DIAG_LOG

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "storage.h"
#include "version.h"
#include "core_alloc.h"

#define DIAG_LOG_PATH "/.crazypod/diag.log"
/* Small enough to paste into a message, large enough for a session. */
#define DIAG_LOG_LIMIT (48 * 1024)
#define DIAG_BUFFER_SIZE 3072
/* Flush before an append can be refused for want of room. */
#define DIAG_HIGH_WATER (DIAG_BUFFER_SIZE - 256)
/* Long enough that a burst costs one write, short enough that losing
 * power never costs more than the last few seconds of diagnostics. */
#define DIAG_FLUSH_INTERVAL (20 * HZ)

/*
 * Lines are held in RAM and written only when the disk is already awake,
 * or when the buffer is nearly full -- the same rule the performance log
 * follows, and for the same reason: waking a sleeping ATA device costs
 * most of a second, and a caller has no idea how often it is on a hot
 * path.
 *
 * The first version of this wrote every line the moment it was produced,
 * which was survivable for a book opening and ruinous for artwork: a
 * library whose covers all fail to decode turned every failure into an
 * open, a write and a close, one per album, while scrolling.
 */
static char pending[DIAG_BUFFER_SIZE];
static size_t pending_length;
static bool flushing;
static bool stopped;
static long last_flush;

static void flush_pending(void)
{
    size_t written = pending_length;
    int fd;

    if(written == 0 || flushing || stopped)
        return;
    if(!dir_exists("/.crazypod") && mkdir("/.crazypod") < 0)
        return;
    flushing = true;
    fd = open(DIAG_LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if(fd >= 0) {
        /* Start over rather than grow without bound: the interesting
         * lines are from the run about to be reported. */
        if(filesize(fd) > DIAG_LOG_LIMIT) {
            close(fd);
            fd = open(DIAG_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        }
    }
    if(fd < 0) {
        flushing = false;
        return;
    }
    write(fd, pending, written);
    close(fd);
    /* Another thread may have appended while the write was yielding, so
     * drop only the bytes that went out. */
    if(pending_length > written)
        memmove(pending, pending + written, pending_length - written);
    pending_length -= written;
    last_flush = current_tick;
    flushing = false;
}

/*
 * Held lines have to reach the disk on their own eventually. Waiting for
 * the buffer to fill is fine on a machine that keeps running; it is not
 * fine when the device is switched off, which is how a whole session's
 * diagnostics went missing once already.
 */
void crazypod_diag_log_service(void)
{
    static long last_audit;

    /*
     * The arena walk is cheap but not free, and this runs on every UI
     * tick. Once every twenty seconds still names the twenty-second
     * window the damage happened in, which is what it is for.
     */
    if(!TIME_BEFORE(current_tick, last_audit + DIAG_FLUSH_INTERVAL)) {
        last_audit = current_tick;
        (void)crazypod_diag_audit_arena("tick");
    }
    if(pending_length == 0 || flushing || stopped)
        return;
    if(TIME_BEFORE(current_tick, last_flush + DIAG_FLUSH_INTERVAL))
        return;
    flush_pending();
}

void crazypod_diag_log(const char *tag, const char *format, ...)
{
    char line[192];
    va_list arguments;
    int prefix;
    int written;
    /*
     * Which build produced these lines. Without it a log and a report can
     * be read against the wrong firmware, which has already happened.
     */
    static bool stamped;

    if(tag == NULL || format == NULL || stopped)
        return;
    if(!stamped) {
        stamped = true;
        crazypod_diag_log("build", "%s", rbversion);
    }
    prefix = snprintf(line, sizeof(line), "%ld %s ",
                      (long)(current_tick / HZ), tag);
    if(prefix < 0 || (size_t)prefix >= sizeof(line))
        return;
    va_start(arguments, format);
    written = vsnprintf(line + prefix, sizeof(line) - prefix - 1,
                        format, arguments);
    va_end(arguments);
    if(written < 0)
        return;
    written += prefix;
    if((size_t)written > sizeof(line) - 2)
        written = (int)sizeof(line) - 2;
    line[written++] = '\n';

    if(pending_length + (size_t)written > sizeof(pending)) {
        /* Nowhere to put it and the disk is asleep: lose the line rather
         * than stall whatever thread produced it. */
        if(flushing)
            return;
        flush_pending();
        if(pending_length + (size_t)written > sizeof(pending))
            return;
    }
    memcpy(pending + pending_length, line, (size_t)written);
    pending_length += (size_t)written;

    if(pending_length >= DIAG_HIGH_WATER || storage_disk_is_active())
        flush_pending();
}

void crazypod_diag_log_flush(void)
{
    flush_pending();
}

bool crazypod_diag_audit_arena(const char *where)
{
    static bool reported;
    size_t blocks = 0;
    void *bad = NULL;

    if(core_audit(&blocks, &bad))
        return true;
    /*
     * One line, not one per tick: once the arena is broken every later
     * audit fails too, and the first one is the one that says when.
     */
    if(!reported) {
        reported = true;
        crazypod_diag_log(
            "arena", "%s: block list breaks after %lu blocks at %p",
            where != NULL ? where : "?", (unsigned long)blocks, bad);
        crazypod_diag_log_flush();
    }
    return false;
}

#endif
