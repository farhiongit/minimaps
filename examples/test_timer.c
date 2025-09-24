#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "timer.h"

static double TWO = 2;
static double THREE = 3;

static void
hello (void *arg)
{
  printf ("\nTimer callback: Hello (after %g seconds since creation).\n\n", *(double *) arg);
}

int
main (void)
{
#define logtime do { timespec_get (&t, TIME_UTC); printf ("%.3f s: ", difftime (t.tv_sec, t0.tv_sec) + 1.e-9 * (double) (t.tv_nsec - t0.tv_nsec)); } while (0)
  struct timespec t0, t;
  timespec_get (&t0, TIME_UTC);
  logtime;
  printf ("Set timer in %g seconds from start.\n", TWO);
  void *timera = timer_set (delay_to_abs_timespec (TWO), hello, &TWO);
  if (!timera)
    fprintf (stderr, "ERROR: Could not set the timer ending in %g seconds.\n", TWO);
  logtime;
  printf ("Set timer in %g seconds from start.\n", THREE);
  void *timerb = timer_set (delay_to_abs_timespec (THREE), hello, &THREE);
  if (!timerb)
    fprintf (stderr, "ERROR: Could not set the timer ending in %g seconds.\n", THREE);
  logtime;
  printf ("Wait %g seconds from now.\n", 1.);
  sleep (1);
  logtime;
  printf ("Remove the timer ending in %g seconds from start.\n", TWO);
  if (!timer_unset (timera))
    fprintf (stderr, "ERROR: Could not remove the timer ending in %g seconds.\n", TWO);
  logtime;
  printf ("Wait %g seconds from now.\n", 3.);
  sleep (3);                    // Keep the program alive until the timer has timed out.
  logtime;
  printf ("Exit.\n");
}
