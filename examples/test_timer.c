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
  printf ("Set timer in %g seconds from start.\n", TWO);
  void *timera = timer_set (delay_to_abs_timespec (TWO), hello, &TWO);
  if (!timera)
    fprintf (stderr, "ERROR: Could not set the timer ending in %g seconds.\n", TWO);
  printf ("Set timer in %g seconds from start.\n", THREE);
  void *timerb = timer_set (delay_to_abs_timespec (THREE), hello, &THREE);
  if (!timerb)
    fprintf (stderr, "ERROR: Could not set the timer ending in %g seconds.\n", THREE);
  printf ("Wait %g seconds from now.\n", 1.);
  sleep (1);
  printf ("Remove the timer ending in %g seconds from start.\n", TWO);
  if (!timer_unset (timera))
    fprintf (stderr, "ERROR: Could not remove the timer ending in %g seconds.\n", TWO);
  printf ("Wait %g seconds from now.\n", 3.);
  sleep (3);                    // Keep the program alive until the timer has timed out.
  printf ("Exit after %g seconds from start.\n", 4.);
}
