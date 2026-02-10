// This complete example aggregates, in uniquely identified groups, adjacent points in an unbound square grid (the complexity does not depend on the spread of the grid.)
// - Unbound square grid (Point in pointsInGroups).
// - Adjacent positions are those that touch horizontally, vertically are diagonally (p_is_adjacent).
// - Grid filled with a stream, incrementally (add_point).
// Find groups of adjacent positions.
// The algorithm comes out to be similar to the union-find algorithm for disjoint-set data structure (https://en.wikipedia.org/wiki/Disjoint-set_data_structure).
#define _DEFAULT_SOURCE // for random
#undef NDEBUG           // for assert
#include <assert.h>
#include <stdlib.h>
#include <time.h>       // for clock

//======================== square grid definition =================================
#include "map.h"
typedef struct
{
  long int x, y;
} Point;

static bool
p_is_adjacent (Point a, Point b) {
  static Point adjacent[] = {
    { 1, 0 },
    { 1, 1 }, // Diagonally
  };
  for (size_t i = 0; i < sizeof (adjacent) / sizeof (*adjacent); i++)
    if (
        (a.x == b.x + adjacent[i].x && a.y == b.y + adjacent[i].y)    //
        || (a.x == b.x - adjacent[i].x && a.y == b.y - adjacent[i].y) // Symetry
        || (a.x == b.x + adjacent[i].y && a.y == b.y - adjacent[i].x) // Rotation 90°
        || (a.x == b.x - adjacent[i].y && a.y == b.y + adjacent[i].x) // Rotation 270°
    )
      return true;

  return false;
}

typedef struct
{
  Point origin, end;
} Rectangle;
//========================= find groups (union-find) ================================
typedef struct
{
  Point p;
  size_t group;
  map *owner;
  bool to_be_removed;
} PointInGroup;

static const void *
g_key (void *data) {
  return &(((const PointInGroup *)data)->group);
}

static int
g_comparator (const void *key_a, const void *key_b, void *arg) {
  (void)arg;
  const size_t ga = *(const size_t *)key_a;
  const size_t gb = *(const size_t *)key_b;
  return ga > gb ? 1
                 : (ga < gb ? -1
                            : 0);
}

static int
equals_p (const void *data, void *sel_arg) {
  const PointInGroup *rg = data;
  Point *p = sel_arg;
  return !rg->to_be_removed && rg->p.x == p->x && rg->p.y == p->y;
}

static int
touches_p (const void *data, void *sel_arg) {
  const PointInGroup *rg = data;
  Point *p = sel_arg;
  return !rg->to_be_removed && p_is_adjacent (rg->p, *p);
}

static int
to_be_removed (const void *data, void *sel_arg) {
  const PointInGroup *rg = data;
  (void)sel_arg;
  return rg->to_be_removed;
}

static int
not_to_be_removed (const void *data, void *sel_arg) {
  return !to_be_removed (data, sel_arg);
}

static int
onlyonce_ptr_g (void *data, void *op_arg, int *remove) {
  (void)remove;
  PointInGroup *rg = data;
  size_t **group = (size_t **)op_arg;
  assert (group);
  if (!*group)
    *group = &rg->group;
  else if (**group != rg->group) {
    *group = 0;
    return 0;
  }
  return 1;
}

static int
changegroup_g (void *data, void *new_group, int *remove) {
  PointInGroup *rg = data;
  assert (rg->group != *(size_t *)new_group);
  // A key MUST not be changed in place:
  // - A new rectangle is created in the new group.
  PointInGroup *new_r = malloc (sizeof (*new_r));
  assert (new_r);
  *new_r = *rg;
  new_r->group = *(size_t *)new_group;
  assert (map_insert_data (new_r->owner, new_r));
  // - The rectangle in the old group is marked to be removed (changegroup_g MUST NOT remove any element.)
  rg->to_be_removed = true;
  (void)remove;
  return 1;
}

static int
regroup_g (void *data, void *op_arg, int *remove) {
  (void)remove;
  PointInGroup *rg = data;
  size_t *ptr_g = op_arg;
  assert (ptr_g);
  //  All rectangles of group rg->group have to switch to group r->group:
  // regroup_g does not remove data (with *remove = 1).
  // Since elements MUST NOT be removed other way than with *remove = 1, changegroup_g MUST NOT remove any element.
  size_t oldgroup = rg->group;
  if (oldgroup != *ptr_g)
    map_find_key (rg->owner, &oldgroup, changegroup_g, ptr_g, 0, 0);
  return 1;
}

static void
add_point (map *owner, Point p) {
  static size_t group = 0;
  static int CHECK_DUPLICATES = 1;

  if (CHECK_DUPLICATES && map_traverse (owner, MAP_EXISTS_ONE, 0, equals_p, &p))
    return;

  size_t *ptr_g = 0;
  size_t g;
  if (!map_traverse (owner, onlyonce_ptr_g, &ptr_g, touches_p, &p))
    g = ++group;
  else if (ptr_g)
    g = *ptr_g;
  else {
    g = ++group;
    map_traverse (owner, regroup_g, &g, touches_p, &p);
    map_traverse (owner, MAP_REMOVE_ALL, free, to_be_removed, 0);
  }

  PointInGroup *pg = malloc (sizeof (*pg));
  assert (pg);
  *pg = (PointInGroup){ .p = p, .group = g, .to_be_removed = false, .owner = owner };
  assert (map_insert_data (owner, pg));
}

//========================= display groups ================================
[[maybe_unused]] static int
bbox_r (void *data, void *op_arg, int *remove) {
  (void)remove;
  PointInGroup *rg = data;
  Point p = rg->p;
  Rectangle *bbox = op_arg;
  if (p.x < bbox->origin.x)
    bbox->origin.x = p.x;
  else if (p.x > bbox->end.x)
    bbox->end.x = p.x;
  if (p.y < bbox->origin.y)
    bbox->origin.y = p.y;
  else if (p.y > bbox->end.y)
    bbox->end.y = p.y;
  return 1;
}

[[maybe_unused]] static int
show_point (void *data, void *op_arg, int *remove) {
  (void)op_arg;
  (void)remove;
  PointInGroup *p = data;
  printf ("%zu: {%li, %li}\n", p->group, p->p.x, p->p.y);
  return 1;
}

[[maybe_unused]] static size_t
find_or_traverse (map *m, const void *key, map_operator op, void *op_arg, map_selector sel, void *sel_arg) {
  if (key)
    return map_find_key (m, key, op, op_arg, sel, sel_arg);
  else
    return map_traverse (m, op, op_arg, sel, sel_arg);
}

[[maybe_unused]] static void
display_group (const void *key, void *op_arg) {
  map *owner = op_arg;
  PointInGroup *rg;
  if (!find_or_traverse (owner, key, MAP_GET_ONE, &rg, not_to_be_removed, 0) || !rg)
    return;
  Rectangle bbox = (Rectangle){ rg->p, rg->p };
  find_or_traverse (owner, key, bbox_r, &bbox, not_to_be_removed, 0);
  printf ("(%li,%li)\n", bbox.origin.x, bbox.origin.y);
  for (long int x = bbox.origin.x; x <= bbox.end.x + 2; x++)
    printf ("-");
  printf ("\n");
  for (long int y = bbox.origin.y; y <= bbox.end.y; y++) {
    printf ("|");
    for (long int x = bbox.origin.x; x <= bbox.end.x; x++)
      if (find_or_traverse (owner, key, MAP_GET_ONE, &rg, equals_p, &(Point){ x, y }))
        printf ("%c", 'a' + (char)(rg->group % ('z' - 'a' + 1)));
      else
        printf (" ");
    printf ("|\n");
  }
  for (long int x = bbox.origin.x; x <= bbox.end.x + 2; x++)
    printf ("-");
  printf ("\n");
}

//====================== main ===========================
int
main (void) {
  static const size_t NB_POINTS = 70;
  static const long int NB_LINES = 12;
  static const long int NB_COLS = 12 ;

  clock_t t0 = clock ();
  map *pointsInGroups;
  assert ((pointsInGroups = map_create (g_key, g_comparator, 0, 0))); // Dictionary of disjoint sets (groups)

  for (size_t i = 0; i < NB_POINTS; i++) {
    Point p = { random () % NB_COLS, random () % NB_LINES };
    add_point (pointsInGroups, p); // Add points in disjoint sets of adjacent points.
  }
  printf ("%g seconds.\n", ((double) (clock () - t0 )) / CLOCKS_PER_SEC);

  // Display results.
  // map_traverse (RectanglesInGroups, show_point, 0, not_to_be_removed, 0);
  display_group (0, pointsInGroups);
  printf ("%zu groups:\n", map_traverse_keys (pointsInGroups, 0, 0));
  map_traverse_keys (pointsInGroups, display_group, pointsInGroups);

  map_traverse (pointsInGroups, MAP_REMOVE_ALL, free, 0, 0);
  map_destroy (pointsInGroups);
}
