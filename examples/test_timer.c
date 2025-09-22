#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "timer.h"

static double ONE = 1;
static double TWO = 2;

static void
hello (void *arg)
{
  printf ("\nTimer callback: Hello (after %g seconds).\n\n", *(double *) arg);
}

int
main (void)
{
  printf ("Set timer in %g seconds.\n", ONE);
  void *timer1s = timer_set (delay_to_abs_timespec (ONE), hello, &ONE);
  if (!timer1s)
    fprintf (stderr, "ERROR: Could not set the timer ending in %g seconds.\n", ONE);
  printf ("Set timer in %g seconds.\n", TWO);
  void *timer2s = timer_set (delay_to_abs_timespec (TWO), hello, &TWO);
  if (!timer2s)
    fprintf (stderr, "ERROR: Could not set the timer ending in %g seconds.\n", TWO);
  printf ("Remove the timer ending in %g seconds.\n", ONE);
  if (!timer_unset (timer1s))
    fprintf (stderr, "ERROR: Could not remove the timer ending in %g seconds.\n", ONE);
  (void) timer2s;
  printf ("Wait %g seconds.\n", 3.);
  sleep (3);                    // Keep the program alive until the timer has timed out.
  printf ("Exit after %g seconds.\n", 3.);
}
