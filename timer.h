// # A simple user-interface to define timers within a single process (but possibly several threads)
// (c) L. Farhi, 2024.
// Language: C (C11 or higher).
#ifndef __TIMERS_H__
#  define __TIMERS_H__
#  include <time.h>

// timer_create and timer_settime are POSIX functions that create and set timers, based on signals or threads. They are nevertheless difficult to use.
// This library let the user define timers more easily.

void *timer_set (struct timespec timeout, void (*callback) (void *arg), void *arg);
// creates and starts a timer. When the absolute time `timeout` is reached, the callback function `callback` is called with `arg` passed as argument.
// - Returns a timer id that can be passed to `timer_unset` to cancel a timer.
// - Complexity: log n, where n is the number of timers previously set.

void timer_unset (void *);
// cancels a previously set timer.
// - Complexity: n

struct timespec delay_to_abs_timespec (double seconds);
// is a helper function to convert a delay in seconds relative to the current time on the timer's clock at the time of the call into an absolute time.
// For use to feed the first argument of `timer_set`.

#endif
