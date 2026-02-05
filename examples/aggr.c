// This complete example aggregates, in uniquely identified groups, adjacent points in an unbound square grid (the complexity does not depend on the spread of the grid.)
// - Unbound square grid. Adjacent positions are those that touch horizontally, vertically are diagonally.
// - Grid filled with a stream, incrementally.
// Find groups of adjacent positions.
#define _DEFAULT_SOURCE // for random
#undef NDEBUG           // for assert
#include <assert.h>
#include <stdlib.h>

//=========================================================
#include "map.h"
typedef struct
{
  long int x, y;
} Point;

typedef struct
{
  Point origin, end;
} Rectangle;

static bool
r_intersects (Rectangle r1, Rectangle r2) {
  return !(r2.origin.x > r1.end.x || r2.end.y < r1.origin.y || r1.origin.x > r2.end.x || r1.end.y < r2.origin.y); // ??
}

static bool
r_is_contiguous (Rectangle r1, Rectangle r2) {
  return r_intersects (r1, (Rectangle){ { r2.origin.x - 1, r2.origin.y - 1 }, { r2.end.x + 1, r2.end.y + 1 } });
}

#define min(a, b) ((a) > (b) ? (b) : (a))
#define max(a, b) ((a) < (b) ? (b) : (a))
static Rectangle
r_union (Rectangle r1, Rectangle r2) {
  return (Rectangle){
    (Point){ min (r1.origin.x, r2.origin.x), min (r1.origin.y, r2.origin.y) },
    (Point){ max (r1.end.x, r2.end.x), max (r1.end.y, r2.end.y) }
  };
}

static bool
r_is_inside (Rectangle r1, Rectangle r2) { // Is r1 inside r2 ?
  return r1.origin.x >= r2.origin.x && r1.end.x <= r2.end.x && r1.origin.y >= r2.origin.y && r1.end.y <= r2.end.y;
}

static bool
r_complements (Rectangle r1, Rectangle r2) {                                                    // Is the union of r1 and r2 a rectangle ?
  return (r1.origin.y == r2.origin.y && r1.end.y == r2.end.y && r1.end.x + 1 == r2.origin.x)    /* r1 to the left of r2 */
         || (r1.origin.y == r2.origin.y && r1.end.y == r2.end.y && r1.origin.x == r2.end.x + 1) /* to the right */
         || (r1.origin.x == r2.origin.x && r1.end.x == r2.end.x && r1.end.y + 1 == r2.origin.y) /* above */
         || (r1.origin.x == r2.origin.x && r1.end.x == r2.end.x && r1.origin.y == r2.end.y + 1) /* below */;
}

//=========================================================
typedef struct
{
  Rectangle r;
  size_t group;
  map *owner;
  bool to_be_removed;
} RectangleInGroup;

static const void *
g_key (void *data) {
  return &(((const RectangleInGroup *)data)->group);
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
includes_r (const void *data, void *sel_arg) {
  const RectangleInGroup *rg = data;
  Rectangle *r = sel_arg;
  return !rg->to_be_removed && r_is_inside (*r, rg->r);
}

static int
matches_r (const void *data, void *sel_arg) {
  const RectangleInGroup *rg = data;
  Rectangle *r = sel_arg;
  return !rg->to_be_removed && r_complements (rg->r, *r);
}

static int
touches_r (const void *data, void *sel_arg) {
  const RectangleInGroup *rg = data;
  Rectangle *r = sel_arg;
  return !rg->to_be_removed && r_is_contiguous (rg->r, *r);
}

static int
to_be_removed (const void *data, void *sel_arg) {
  const RectangleInGroup *rg = data;
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
  RectangleInGroup *rg = data;
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
  RectangleInGroup *rg = data;
  assert (rg->group != *(size_t *)new_group);
  // A key MUST not be changed in place:
  // - A new rectangle is created in the new group.
  RectangleInGroup *new_r = malloc (sizeof (*new_r));
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
  RectangleInGroup *rg = data;
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

static int
bbox_g (void *data, void *op_arg, int *remove) {
  (void)remove;
  RectangleInGroup *rg = data;
  Rectangle *bbox = op_arg;
  *bbox = r_union (rg->r, *bbox);
  return 1;
}

static void
r_add (map *owner, Rectangle r) {
  static size_t group = 0;
  static int CHECK_DUPLICATES = 1;

  if (CHECK_DUPLICATES && map_traverse (owner, MAP_EXISTS_ONE, 0, includes_r, &r))
    return;

  RectangleInGroup *rg = 0;
  if (map_traverse (owner, MAP_REMOVE_ONE, &rg, matches_r, &r) && rg) {
    r_add (owner, r_union (r, rg->r));
    free (rg);
    return;
  }

  size_t *ptr_g = 0;
  size_t g;
  if (!map_traverse (owner, onlyonce_ptr_g, &ptr_g, touches_r, &r))
    g = ++group;
  else if (ptr_g)
    g = *ptr_g;
  else {
    g = ++group;
    map_traverse (owner, regroup_g, &g, touches_r, &r);
    map_traverse (owner, MAP_REMOVE_ALL, free, to_be_removed, 0);
  }

  RectangleInGroup *p = malloc (sizeof (*p));
  assert (p);
  *p = (RectangleInGroup){ .r = r, .group = g, .to_be_removed = false, .owner = owner };
  assert (map_insert_data (owner, p));
}

//=========================================================
static int
show_rectangle (void *data, void *op_arg, int *remove) {
  (void)op_arg;
  (void)remove;
  RectangleInGroup *r = data;
  printf ("%zu: {%li, %li} -- {%li, %li}\n", r->group, r->r.origin.x, r->r.origin.y, r->r.end.x, r->r.end.y);
  return 1;
}

[[maybe_unused]] static void
show_group (const void *key, void *op_arg) {
  map *owner = op_arg;
  printf ("%zu: ...\n", *(const size_t *)key);
  map_find_key (owner, key, show_rectangle, 0, not_to_be_removed, 0);
}

static size_t
find_or_traverse (map *m, const void *key, map_operator op, void *op_arg, map_selector sel, void *sel_arg) {
  if (key)
    return map_find_key (m, key, op, op_arg, sel, sel_arg);
  else
    return map_traverse (m, op, op_arg, sel, sel_arg);
}

static void
display_group (const void *key, void *op_arg) {
  map *owner = op_arg;
  RectangleInGroup *rg;
  if (!find_or_traverse (owner, key, MAP_GET_ONE, &rg, not_to_be_removed, 0) || !rg)
    return;
  Rectangle bbox = rg->r;
  find_or_traverse (owner, key, bbox_g, &bbox, not_to_be_removed, 0);
  printf ("(%li,%li)\n", bbox.origin.x, bbox.origin.y);
  for (long int x = bbox.origin.x; x <= bbox.end.x + 2; x++)
    printf ("-");
  printf ("\n");
  for (long int y = bbox.origin.y; y <= bbox.end.y; y++) {
    printf ("|");
    for (long int x = bbox.origin.x; x <= bbox.end.x; x++)
      if (find_or_traverse (owner, key, MAP_GET_ONE, &rg, includes_r, &(Rectangle){ { x, y }, { x, y } }))
        printf ("%c", 'a' + (char)(rg->group % ('z' - 'a' + 1)));
      else
        printf (" ");
    printf ("|\n");
  }
  for (long int x = bbox.origin.x; x <= bbox.end.x + 2; x++)
    printf ("-");
  printf ("\n");
}

#define NB_ADD 70
#define NB_LINES 12
#define NB_COLS 12
int
main (void) {
  map *RectanglesInGroups;
  assert ((RectanglesInGroups = map_create (g_key, g_comparator, 0, 0))); // Dictionary of groups

  for (size_t i = 0; i < NB_ADD; i++) {
    Point p = { random () % NB_COLS, random () % NB_LINES };
    r_add (RectanglesInGroups, (Rectangle){ p, p });
  }

  // map_traverse (RectanglesInGroups, show_rectangle, 0, not_to_be_removed, 0);
  display_group (0, RectanglesInGroups);
  printf ("%zu groups:\n", map_traverse_keys (RectanglesInGroups, 0, 0));
  map_traverse_keys (RectanglesInGroups, display_group, RectanglesInGroups);

  map_traverse (RectanglesInGroups, MAP_REMOVE_ALL, free, 0, 0);
  map_destroy (RectanglesInGroups);
}
