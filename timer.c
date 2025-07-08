// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <threads.h>
#include "map.h"
#include "timer.h"

struct timer_elem
{
  struct timespec timeout;
  void (*callback) (void *arg);
  void *arg;
};

static const void *
timer_get_key (void *pa)
{
  struct timer_elem *a = pa;
  return &a->timeout;
}

static int
timer_cmp_key (const void *pa, const void *pb, void *arg)
{
  (void) arg;
  const struct timespec *a = pa;
  const struct timespec *b = pb;
  return (a->tv_sec < b->tv_sec ? -1 : a->tv_sec > b->tv_sec ? 1 : a->tv_nsec < b->tv_nsec ? -1 : a->tv_nsec > b->tv_nsec ? 1 : 0);
}

static struct                   // Ordered list of struct timer_elem stored in an ordered binary tree.
{
  thrd_t thread;
  mtx_t mutex;
  cnd_t condition;
  map *map;
  int stop;
} Timers = { 0 };

static void *
Timers_add (struct timespec timeout, void (*callback) (void *arg), void *arg)
{
  struct timer_elem *new = calloc (1, sizeof (*new));
  if (!new)
    return 0;
  new->timeout = timeout;
  new->callback = callback;
  new->arg = arg;

  map_insert_data (Timers.map, new);
  cnd_broadcast (&Timers.condition);
  return new;
}

static int
timer_by_id (const void *data, void *timer)
{
  return data == timer;
}

static void
Timers_rm (void *timer)
{
  if (map_traverse (Timers.map, MAP_REMOVE_ONE, &timer, timer_by_id, timer))
  {
    free (timer);
    cnd_broadcast (&Timers.condition);
  }
}

static int
Timers_loop (void *)
{
  mtx_lock (&Timers.mutex);
  while (!Timers.stop)
  {
    struct timer_elem *earliest = 0;
    map_traverse (Timers.map, MAP_REMOVE_ONE, &earliest, 0, 0);
    if (!earliest)
      cnd_wait (&Timers.condition, &Timers.mutex);
    else if (cnd_timedwait (&Timers.condition, &Timers.mutex, &earliest->timeout) != thrd_timedout)
      map_insert_data (Timers.map, earliest);
    else
    {
      if (earliest->callback)
        earliest->callback (earliest->arg);
      free (earliest);
    }
  }
  mtx_unlock (&Timers.mutex);
  return 0;
}

static void
Timers_clear (void)
{
  Timers.stop = 1;
  cnd_broadcast (&Timers.condition);
  thrd_join (Timers.thread, 0);
  map_traverse (Timers.map, MAP_REMOVE_ALL, free, 0, 0);
  map_destroy (Timers.map);
  cnd_destroy (&Timers.condition);
  mtx_destroy (&Timers.mutex);
}

static once_flag TIMERS_INIT = ONCE_FLAG_INIT;
static void
Timers_init (void)              // Called once.
{
  if (!(Timers.map = map_create (timer_get_key, timer_cmp_key, 0, 0)))
    return;
  mtx_init (&Timers.mutex, mtx_plain);
  cnd_init (&Timers.condition);
  thrd_create (&Timers.thread, Timers_loop, 0); // The thread won't be destroyed and will never end. It will be stopped when the thread of the caller to timer_set will end.
  atexit (Timers_clear);
}

struct timespec
delay_to_abs_timespec (double seconds)
{
  long sec = (long) (seconds);
  long nsec = (long) (seconds * 1000 * 1000 * 1000) - (sec * 1000 * 1000 * 1000);
  struct timespec t;
  timespec_get (&t, TIME_UTC);  // C standard function, returns now. UTC since cnd_timedwait is UTC-based.
  t.tv_sec += sec + (t.tv_nsec + nsec) / (1000 * 1000 * 1000);
  t.tv_nsec = (t.tv_nsec + nsec) % (1000 * 1000 * 1000);
  return t;
}

void *
timer_set (struct timespec timeout, void (*callback) (void *arg), void *arg)
{
  call_once (&TIMERS_INIT, Timers_init);
  return Timers_add (timeout, callback, arg);
}

void
timer_unset (void *timer)
{
  if (Timers.map)
    Timers_rm (timer);
}
