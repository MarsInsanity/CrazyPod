#ifndef CRAZYPOD_DIAG_LOG_H
#define CRAZYPOD_DIAG_LOG_H

#include <stdbool.h>

#include "config.h"

/*
 * Bring-up diagnostics for bugs that only happen on a device, with the
 * owner's own files.
 *
 * Two of them -- an audiobook whose progress bar is wrong and a Game Boy
 * save that never comes back -- survived four rounds of reading the code
 * and guessing, at a test pass each. An on-screen line did not close the
 * gap either: it has to be read, copied and typed back, and the one that
 * matters is a line of hexadecimal.
 *
 * A file does close it, because /.crazypod/perf.log already comes back
 * every round without being asked twice. This appends plain lines to
 * /.crazypod/diag.log: rare events only, opened and closed per line, and
 * capped so it cannot grow without bound.
 */
#if defined(HAVE_CRAZYPOD_UI) && !defined(SIMULATOR)
#define CRAZYPOD_DIAG_LOG
void crazypod_diag_log(const char *tag, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
/* Push whatever is buffered to disk now, for a clean shutdown or before
 * something that is about to make the device unreachable. */
void crazypod_diag_log_flush(void);

/*
 * Walk the buflib arena and log it if the block list no longer adds up.
 * Returns true while the arena is intact. "where" names the operation
 * that has just finished, so the first audit that fails names the
 * operation that broke it.
 */
bool crazypod_diag_audit_arena(const char *where);
/* Called from the runtime tick: writes held lines out once they have
 * waited long enough, so a power-off never costs more than a few
 * seconds of them. Does nothing when there is nothing held. */
void crazypod_diag_log_service(void);
#else
static inline void crazypod_diag_log(const char *tag, const char *format, ...)
{
    (void)tag;
    (void)format;
}
static inline void crazypod_diag_log_flush(void) {}
static inline void crazypod_diag_log_service(void) {}
#endif

#endif
