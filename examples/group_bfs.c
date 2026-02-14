// This complete example aggregates, in uniquely identified groups, adjacent points in an unbound square grid (the complexity does not depend on the spread of the grid.)
// - Unbound square grid (Point in pointsInGroups).
// - Adjacent positions are those that touch horizontally, vertically are diagonally (p_is_adjacent).
// - Grid initially filled with a finite number of points.
// Find groups of adjacent positions.
// The algorithm uses a BFS.
#include <stdlib.h>
//============================== Genericity ============================================
#define TWO ADD (ONE, ONE)
#define GT(a, b) LT ((b), (a))
#define LE(a, b) (!(GT ((a), (b))))
#define GE(a, b) (!(LT ((a), (b))))
//======================== square grid definition =================================
#include "map.h"
typedef struct
{
  TYPE x, y;
} Point;

[[maybe_unused]] static int
p_comparator (const void *key_a, const void *key_b, void *arg) {
  (void)arg;
  const Point pa = *(const Point *)key_a;
  const Point pb = *(const Point *)key_b;
  return GT (pa.y, pb.y) ? 1 : (LT (pa.y, pb.y) ? -1 : (GT (pa.x, pb.x) ? 1 : (LT (pa.x, pb.x) ? -1 : 0)));
}

//======================= find groups (BFS) ================================
typedef struct {
  map *grid;
  map *current_group;
} Visit_args;

[[maybe_unused]] static int
visit_group (void *data, void *op_arg, int *remove) {
  static const Point adjacent[] = {
    { ONE, ZERO },
    { ONE, ONE }, // Diagonally
  };

  (void)remove;
  Point *current_point_in_group = data;
  Visit_args *args = op_arg;
  map *grid = args->grid;
  map *current_group = args->current_group;

  Point *current_point_in_grid;
  for (size_t i = 0; i < sizeof (adjacent) / sizeof (*adjacent); i++) // Breadth First Search
    for (size_t j = 0; j < 4; j++) /*NSWE*/ {
      Point delta = j == 0 ? (Point){ adjacent[i].x, adjacent[i].y } : (j == 1 ? (Point){ MINUS (adjacent[i].x), MINUS (adjacent[i].y) } : (j == 2 ? (Point){ adjacent[i].y, MINUS (adjacent[i].x) } : (Point){ MINUS (adjacent[i].y), adjacent[i].x }));
      // Find each point in the grid which is adjacent to the point in the current group (the grid is an ordered set.)
      // Remove this point from the grid.
      if (map_find_key (grid, &(Point){ ADD (current_point_in_group->x, delta.x), ADD (current_point_in_group->y, delta.y) }, MAP_REMOVE_ONE, &current_point_in_grid, 0, 0)) // (Complexity O(log(n)))
        map_insert_data (current_group, current_point_in_grid);                                                                                                              // Append it at the end of the current group (the very same group that is being traversed.)
    }
  return 1;
}

[[maybe_unused]] static size_t
grid_to_groups (map *grid, map *groups) {
  Point *current_point_in_group = 0;
  while (map_traverse (grid, MAP_REMOVE_ONE, &current_point_in_group, 0, 0)) // Remove a point from the grid.
  {
    map *current_group;
    map_insert_data (groups, current_group = map_create (0, 0, 0, 0));                                              // Create a new group.
    map_insert_data (current_group, current_point_in_group);                                                        // Add the point to the current group.
    map_traverse (current_group, visit_group, &(Visit_args){ .grid = grid, .current_group = current_group }, 0, 0); // Traverse the points of the current group, from beginning to end (see visit_group.) (Complexity O(n))
  }
  return map_size (groups);
}

[[maybe_unused]] static void
free_group (void *g) {
  map_traverse (g, MAP_REMOVE_ALL, free, 0, 0);
  free (g);
}

//======================= display groups ================================
typedef struct {
  Point origin, end;
} Rectangle;

[[maybe_unused]] static int
bbox_r (void *data, void *op_arg, int *remove) {
  (void)remove;
  Point p = *(Point *)data;
  Rectangle *bbox = op_arg;
  if (LT (p.x, bbox->origin.x))
    bbox->origin.x = p.x;
  else if (GT (p.x, bbox->end.x))
    bbox->end.x = p.x;
  if (LT (p.y, bbox->origin.y))
    bbox->origin.y = p.y;
  else if (GT (p.y, bbox->end.y))
    bbox->end.y = p.y;
  return 1;
}

[[maybe_unused]] static int
equals_p (const void *data, void *sel_arg) {
  const Point *rg = data;
  Point *p = sel_arg;
  return EQ (rg->x, p->x) && EQ (rg->y, p->y);
}

[[maybe_unused]] static int
display_group (void *group, void *op_arg, int *remove) {
  (void)remove;
  (void)op_arg;
  Point *rg;
  if (!map_traverse (group, MAP_GET_ONE, &rg, 0, 0) || !rg)
    return 1;
  Rectangle bbox = (Rectangle){ *rg, *rg };
  map_traverse (group, bbox_r, &bbox, 0, 0);
  printf ("Group at (" FORMAT "," FORMAT "), with %zu points:\n", bbox.origin.x, bbox.origin.y, map_size (group));
  for (TYPE x = bbox.origin.x; LE (x, ADD (bbox.end.x, TWO)); x = ADD (x, ONE))
    printf ("-");
  printf ("\n");
  for (TYPE y = bbox.origin.y; LE (y, bbox.end.y); y = ADD (y, ONE)) {
    printf ("|");
    for (TYPE x = bbox.origin.x; LE (x, bbox.end.x); x = ADD (x, ONE))
      printf ("%c", map_traverse (group, MAP_GET_ONE, &rg, equals_p, &(Point){ x, y }) ? '*' : ' ');
    printf ("|\n");
  }
  for (TYPE x = bbox.origin.x; LE (x, ADD (bbox.end.x, TWO)); x = ADD (x, ONE))
    printf ("-");
  printf ("\n");
  return 1;
}
