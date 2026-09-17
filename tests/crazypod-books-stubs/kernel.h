#ifndef CRAZYPOD_BOOKS_STUB_KERNEL_H
#define CRAZYPOD_BOOKS_STUB_KERNEL_H

/*
 * The catalog times its epub probes, which on the device come from the
 * kernel tick. The host test never probes an epub, so a tick that does
 * not advance is enough: it only has to compile and to read as "no time
 * has passed", which keeps the slow-probe line out of the test's output.
 */
#define HZ 100
extern long current_tick;

#endif
