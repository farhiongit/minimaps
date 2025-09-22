// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <threads.h>
#include "map.h"
#include "timer.h"

#define map_display(...)

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

static void
displayer (FILE *stream, const void *data)
{
  struct timespec t0;
  timespec_get (&t0, TIME_UTC); // C standard function, returns now. UTC since cnd_timedwait is UTC-based.
  const struct timer_elem *timer = data;
  fprintf (stream, "%g", (double) (timer->timeout.tv_sec - t0.tv_sec) + 1e-9 * (double) (timer->timeout.tv_nsec - t0.tv_nsec));
}

static void *
Timers_add (struct timespec timeout, void (*callback) (void *arg), void *arg)
{
  struct timer_elem *new = calloc (1, sizeof (*new));
  if (!new)
    return 0;
  new->timeout = timeout;
  new->callback = callback;
  new->arg = arg;

  if (!Timers.map || !map_insert_data (Timers.map, new))
  {
    free (new);
    return 0;
  }
  else
  {
    map_display (Timers.map, stderr, displayer);
    cnd_broadcast (&Timers.condition);
    return new;
  }
}

static int
timer_by_id (const void *data, void *timer)
{
  return data == timer;
}

static int
Timers_rm (void *timer)
{
  void *t;
  if (Timers.map && map_traverse (Timers.map, MAP_REMOVE_ONE, &t, timer_by_id, timer))
  {
    map_display (Timers.map, stderr, displayer);
    free (t);
    cnd_broadcast (&Timers.condition);
    return 1;
  }
  return 0;
}

static int
Timers_loop (void *)
{
  while (!Timers.stop)
  {
    mtx_lock (&Timers.mutex);
    struct timer_elem *earliest = 0;
    if (!map_traverse (Timers.map, MAP_REMOVE_ONE, &earliest, 0, 0) || !earliest)
      cnd_wait (&Timers.condition, &Timers.mutex);
    else
    {
      map_insert_data (Timers.map, earliest);
      map_display (Timers.map, stderr, displayer);
      if (cnd_timedwait (&Timers.condition, &Timers.mutex, &earliest->timeout) == thrd_timedout)
      {
        if (earliest->callback)
          earliest->callback (earliest->arg);
        Timers_rm (earliest);
      }
    }
    mtx_unlock (&Timers.mutex);
  }
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
  (void) displayer;
  mtx_init (&Timers.mutex, mtx_plain);
  cnd_init (&Timers.condition);
  mtx_lock (&Timers.mutex);
  if ((Timers.map = map_create (timer_get_key, timer_cmp_key, 0, 0)))
    thrd_create (&Timers.thread, Timers_loop, 0);       // The thread won't be destroyed and will never end. It will be stopped when the thread of the caller to timer_set will end.
  mtx_unlock (&Timers.mutex);
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
  cnd_broadcast (&Timers.condition);
  mtx_lock (&Timers.mutex);
  void *ret = Timers_add (timeout, callback, arg);
  mtx_unlock (&Timers.mutex);
  return ret;
}

int
timer_unset (void *timer)
{
  cnd_broadcast (&Timers.condition);
  mtx_lock (&Timers.mutex);
  int ret = Timers_rm (timer);
  mtx_unlock (&Timers.mutex);
  return ret;
}
