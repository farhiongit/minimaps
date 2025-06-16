#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "timer.h"

static double ONE = 1;
static double TWO = 2;

static void
hello (void *arg)
{
  printf ("Hello (after %f seconds).\n", *(double *)arg);
  exit (EXIT_SUCCESS);
}

int main (void)
{
  void *timer1s = timer_set (delay_to_abs_timespec (ONE), hello, &ONE);
  void *timer2s = timer_set (delay_to_abs_timespec (TWO), hello, &TWO);
  timer_unset (timer1s);
  (void) timer2s;
  sleep (100);
}
