#define _DEFAULT_SOURCE // for random
#include <stdlib.h>
#include <time.h>       // for clock
#include "grid_type.h"  // for group_bfs.c
#include "group_bfs.c"
#include "map.h"
//================================= Test ===================================
int
main (void) {
  static const size_t NB_POINTS = 70;
  static const long int NB_LINES = 12;
  static const long int NB_COLS = 12;

  map *grid = map_create (0, p_comparator, 0, 1); // The grid is an ordered set.
  for (size_t i = 0; i < NB_POINTS; i++) {
    Point *p = malloc (sizeof (*p));
    *p = (Point){ random () % NB_COLS, random () % NB_LINES };
    if (!map_insert_data (grid, p))
      free (p);
  }
  display_group (grid, 0, 0);

  clock_t t0 = clock ();
  map *groups = map_create (0, 0, 0, 0);
  printf ("%zu groups found in %g seconds.\n", grid_to_groups (grid, groups), ((double)(clock () - t0)) / CLOCKS_PER_SEC);

  map_traverse (groups, display_group, 0, 0, 0);

  map_traverse (groups, MAP_REMOVE_ALL, free_group, 0, 0);
  map_destroy (groups);
  map_destroy (grid);
}
