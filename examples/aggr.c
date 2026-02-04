// This complete example aggregates, in uniquely identified groups, adjacent points in an unbound square grid (the complexity does not depend on the spread of the grid.)
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
p_is_inside (Point p, Rectangle r) {
  return p.x >= r.origin.x && p.x <= r.end.x && p.y >= r.origin.y && p.y <= r.end.y;
}

static bool
r_intersects (Rectangle r1, Rectangle r2) {
  return p_is_inside (r1.origin, r2) || p_is_inside (r1.end, r2) || p_is_inside ((Point){ r1.origin.x, r1.end.y }, r2) || p_is_inside ((Point){ r1.end.x, r1.origin.y }, r2);
}

static bool
r_is_contiguous (Rectangle r1, Rectangle r2) {
  return r_intersects (r1, (Rectangle){ { r2.origin.x - 1, r2.origin.y - 1 }, { r2.end.x + 1, r2.end.y + 1 } });
}

#define min(a, b) (a > b ? b : a)
#define max(a, b) (a < b ? b : a)
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
static map *RectanglesInGroups = 0;

typedef struct
{
  Rectangle r;
  size_t group;
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
  assert (map_insert_data (RectanglesInGroups, new_r));
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
    map_find_key (RectanglesInGroups, &oldgroup, changegroup_g, ptr_g, 0, 0);
  return 1;
}

// Add a new Rectangle r1:
// - traverse the list and search for complements of r1 (r_complements)
// - if found:
//   - remove it from the list
//   - complement it (r_union) to the new Rectangle r1 into r2
//   - Add r2 to the list (recursively).
// Hypothesis: r1 is not in the set yet.
static void
r_add (Rectangle r) {
  static size_t group = 0;
  static int CHECK_DUPLICATES = 1;

  if (CHECK_DUPLICATES && map_traverse (RectanglesInGroups, MAP_EXISTS_ONE, 0, includes_r, &r))
    return;

  RectangleInGroup *rg = 0;
  if (map_traverse (RectanglesInGroups, MAP_REMOVE_ONE, &rg, matches_r, &r) && rg) {
    r_add (r_union (r, rg->r));
    free (rg);
    return;
  }

  size_t *ptr_g = 0;
  size_t g;
  if (!map_traverse (RectanglesInGroups, onlyonce_ptr_g, &ptr_g, touches_r, &r))
    g = ++group;
  else if (ptr_g)
    g = *ptr_g;
  else {
    g = ++group;
    map_traverse (RectanglesInGroups, regroup_g, &g, touches_r, &r);
    map_traverse (RectanglesInGroups, MAP_REMOVE_ALL, free, to_be_removed, 0);
  }

  RectangleInGroup *p = malloc (sizeof (*p));
  assert (p);
  *p = (RectangleInGroup){ .r = r, .group = g, .to_be_removed = false };
  assert (map_insert_data (RectanglesInGroups, p));
}

static int
display (void *data, void *op_arg, int *remove) {
  (void)op_arg;
  (void)remove;
  RectangleInGroup *r = data;
  printf ("%zu: {%li, %li} -- {%li, %li}\n", r->group, r->r.origin.x, r->r.origin.y, r->r.end.x, r->r.end.y);
  return 1;
}

[[maybe_unused]] static void
show_group (const void *key, void *op_arg) {
  (void)op_arg;
  printf ("%zu: ...\n", *(const size_t *)key);
  map_find_key (RectanglesInGroups, key, display, 0, not_to_be_removed, 0);
}

//=========================================================
#define NB_ADD 40
#define NB_LINES 10
#define NB_COLS 10
static void
display_grid (const void *key, void *op_arg) {
  (void)op_arg;
  const size_t *group = key;
  RectangleInGroup *rg;
  for (long int x = 0; x < NB_COLS + 2; x++)
    printf ("-");
  printf ("\n");
  for (long int y = 0; y < NB_LINES; y++) {
    printf ("|");
    for (long int x = 0; x < NB_COLS; x++)

      if ((!group && map_traverse (RectanglesInGroups, MAP_GET_ONE, &rg, includes_r, &(Rectangle){ { x, y }, { x, y } }))
          || (group && map_find_key (RectanglesInGroups, group, MAP_GET_ONE, &rg, includes_r, &(Rectangle){ { x, y }, { x, y } })))
        printf ("%c", 'a' + (char)(rg->group % ('z' - 'a' + 1)));
      else
        printf (" ");
    printf ("|\n");
  }
  for (long int x = 0; x < NB_COLS + 2; x++)
    printf ("-");
  printf ("\n");
}

int
main (void) {
  assert ((RectanglesInGroups = map_create (g_key, g_comparator, 0, 0))); // Dictionary of groups

  for (size_t i = 0; i < NB_ADD; i++) {
    Point p = { random () % NB_COLS, random () % NB_LINES };
    r_add ((Rectangle){ p, p });
  }

  // map_traverse (RectanglesInGroups, display, 0, not_to_be_removed, 0);
  display_grid (0, 0);
  printf ("%zu rectangles in %zu groups.\n", map_size (RectanglesInGroups), map_traverse_keys (RectanglesInGroups, 0, 0));
  map_traverse_keys (RectanglesInGroups, display_grid, 0);
  // map_traverse_keys (RectanglesInGroups, show_group, 0);

  map_traverse (RectanglesInGroups, MAP_REMOVE_ALL, free, 0, 0);
  map_destroy (RectanglesInGroups);
}
