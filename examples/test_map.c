// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#include <stdlib.h>
#include <assert.h>
#include <sys/resource.h>
#include <math.h>
#include <errno.h>
#include <threads.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include "map.h"
#include "trace.h"
#if 1
#  define map_create(...)                 TRACE_EXPRESSION(map_check (map_create (__VA_ARGS__)))
#  define map_destroy(map)                TRACE_EXPRESSION(map_destroy (map_check ((map))))
#  define map_insert_data(map, ...)       TRACE_EXPRESSION(map_insert_data (map_check ((map)), __VA_ARGS__))
#  define map_traverse(map, ...)          TRACE_EXPRESSION(map_traverse (map_check ((map)), __VA_ARGS__))
#  define map_traverse_backward(map, ...) TRACE_EXPRESSION(map_traverse_backward (map_check ((map)), __VA_ARGS__))
#  define map_find_key(map, ...)          TRACE_EXPRESSION(map_find_key (map_check ((map)), __VA_ARGS__))
#  define map_size(map)                   TRACE_EXPRESSION(map_size (map_check ((map))))
#endif

static int
cmpstringp (const void *p1, const void *p2, void *arg)
{
  /* The actual arguments to this function are "pointers to
     pointers to char", but strcmp(3) arguments are "pointers
     to char", hence the following cast plus dereference. */

  (void) arg;
  return strcmp ((const char *) p1, (const char *) p2);
}

static int
print_data (void *data, void *res, int *remove)
{
  (void) res;
  fprintf (stdout, "%s ", (char *) data);
  *remove = 0;                  // Tells: do not remove the data from the map.
  return 1;                     // Tells: continue traversing.
}

static int
select_start_with_c (const void *data, void *res)
{
  char *c = res;
  return (*(const char *) data == *c);
}

static void
tostring (FILE *stream, const void *data)
{
  fprintf (stream, "%s", (const char *) data);
}

static void
test1 (void)
{
  for (int i = 1; i <= 3; i++)
  {
    map *li;
    puts ("============================================================");
    switch (i)
    {
      case 1:
        li = map_create (0, cmpstringp, 0, 1);  // Sorted set
        break;
      case 2:
        li = map_create (0, cmpstringp, 0, 0);  // Sorted list
        break;
      case 3:
        li = map_create (0, 0, 0, 0);   // Unsorted list
        break;
      default:
    }
    map_insert_data (li, "b");  // The map stores pointers to static data of type char[].
    map_insert_data (li, "a");
    map_insert_data (li, "d");
    map_insert_data (li, "c");
    map_insert_data (li, "c");
    map_insert_data (li, "a");
    map_insert_data (li, "aa");
    map_insert_data (li, "cc");
    map_insert_data (li, "d");
    map_insert_data (li, "ba");
    map_display (li, stderr, tostring);

    map_traverse (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");
    map_traverse_backward (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");
    char c = 'c';
    map_traverse (li, print_data, 0, select_start_with_c, &c);
    fprintf (stdout, "\n");

    char *data = 0;
    if (map_traverse (li, MAP_REMOVE_ONE, &data, 0, 0) && data) // Remove the first found element from the map.
    {
      map_display (li, stderr, tostring);
      fprintf (stdout, "%s <-- ", data);
      fflush (stdout);
      map_traverse (li, print_data, 0, 0, 0);
      fprintf (stdout, "<-- %s\n", data);
      map_insert_data (li, data);       // Reinsert after use.
      map_display (li, stderr, tostring);
      map_traverse (li, print_data, 0, 0, 0);
      fprintf (stdout, "\n");
    }

    map_insert_data (li, "r");
    map_traverse (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");
    map *lj = map_create (0, 0, 0, 0);
    map_find_key (li, "r", MAP_MOVE_TO, lj);
    map_traverse (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");
    map_traverse (lj, print_data, 0, 0, 0);
    fprintf (stdout, "\n");
    map_traverse (lj, MAP_REMOVE_ALL, 0, 0, 0);
    map_destroy (lj);

    map_display (li, stderr, tostring);
    map_find_key (li, "c", MAP_REMOVE_ALL, 0);
    map_display (li, stderr, tostring);
    map_traverse (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");
    fprintf (stdout, "%lu elements.\n", map_size (li));

    map_traverse (li, MAP_REMOVE_ONE, 0, 0, 0);
    map_traverse (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");

    map_find_key (li, "b", MAP_REMOVE_ONE, 0);
    map_traverse (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");

    map_find_key (li, "d", MAP_REMOVE_ONE, 0);
    map_traverse (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");

    map_traverse_backward (li, MAP_REMOVE_ONE, 0, 0, 0);
    map_traverse (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");

    map_traverse (li, MAP_REMOVE_ONE, 0, 0, 0);
    map_traverse (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");

    map_traverse (li, MAP_REMOVE_ALL, 0, 0, 0);
    map_traverse (li, print_data, 0, 0, 0);
    fprintf (stdout, "\n");
    fprintf (stdout, "%lu elements.\n", map_size (li));
    map_destroy (li);
    fprintf (stdout, "=======\n");
  }
}

static int
cmpip (const void *p1, const void *p2, void *arg)
{
  (void) arg;
  return *(const int *) p1 < *(const int *) p2 ? -1 : *(const int *) p1 > *(const int *) p2 ? 1 : 0;
}

static int
nop (int i)
{
  return i;
}

static int
dbl (int i)
{
  return 2 * i;
}

static int
dec (int i)
{
  return i - 1;
}

static int
apply (void *data, void *res, int *remove)
{
  (void) remove;
  int (*f) (int) = res;
  *(int *) data = f (*(int *) data);
  return 1;                     // Tells: continue traversing.
}

struct rai_args
{
  int (*f) (int);
  map *map;
};

static int
remove_apply_insert (void *data, void *res, int *remove)
{
  int (*f) (int) = ((struct rai_args *) res)->f;
  map *m = ((struct rai_args *) res)->map;
  int *pi = malloc (sizeof (*pi));
  *pi = f (*(int *) data);
  if (map_insert_data (m, pi))
  {
    free (data);
    *remove = 1;                // Tells: remove the data from the map.
  }
  else
    free (pi);
  return 1;                     // Tells: continue traversing.
}

static void
toint (FILE *stream, const void *data)
{
  fprintf (stream, "%i", *(const int *) data);
}

static int
print_pi (void *data, void *res, int *remove)
{
  (void) res;
  toint (stdout, data);
  fprintf (stdout, " ");
  *remove = 0;                  // Tells: do not remove the data from the map.
  return 1;                     // Tells: continue traversing.
}

static void
test2 (void)
{
  puts ("============================================================");
  map *li = map_create (0, cmpip, 0, 0);        // Ordered list
  for (size_t i = 0; i < 10; i++)
  {
    int *pi = malloc (sizeof (*pi));
    *pi = rand () % 39 + 11;
    map_insert_data (li, pi);
  }
  map_traverse (li, print_pi, 0, 0, 0);
  fprintf (stdout, "\n");
  map_traverse (li, apply, dec, 0, 0);
  map_traverse (li, print_pi, 0, 0, 0);
  fprintf (stdout, "\n");
  map_traverse (li, apply, dbl, 0, 0);
  map_traverse (li, print_pi, 0, 0, 0);
  fprintf (stdout, "\n");
  struct rai_args args;
  args = (struct rai_args)
  { nop, li };
  map_traverse_backward (li, remove_apply_insert, &args, 0, 0);
  map_traverse (li, print_pi, 0, 0, 0);
  fprintf (stdout, "\n");
  args = (struct rai_args)
  { dec, li };
  map_traverse (li, remove_apply_insert, &args, 0, 0);
  map_traverse (li, print_pi, 0, 0, 0);
  fprintf (stdout, "\n");
  args = (struct rai_args)
  { dbl, li };
  map_traverse (li, remove_apply_insert, &args, 0, 0);  // Integers are removed, doubled and pushed back forward into the ordered list and traversed repeatedly, till they overflow to lower negative numbers (and are therefore pushed backward).
  map_traverse (li, print_pi, 0, 0, 0);
  fprintf (stdout, "\n");
  map_traverse (li, MAP_REMOVE_ALL, free, 0, 0);
  map_traverse (li, print_pi, 0, 0, 0);
  fprintf (stdout, "\n");
  map_destroy (li);
}

enum class
{ NOUN, VERB, ADJECTIVE, ADVERB, PRONOUN, DETERMINER, PREPOSITION, CONJUNCTION, INTERJECTION };
enum gender
{ MASCULINE, FEMININE, NEUTER, NONE };
struct entry                    // The type of the data stored in the map
{
  struct word
  {
    char *spelling;
    enum class class;
  } word;
  enum gender gender;
  char *definition;
};

static const void *
get_word (void *data)           // 'data' is supposed to be a pointer to 'struct entry'
{
  return &((const struct entry *) data)->word;  // 'word' is declared as the subset of the 'data' that defines the key of the map.
}

static int
cmp_word (const void *p1, const void *p2, void *arg)
{
  (void) arg;
  const struct word *w1 = p1;
  const struct word *w2 = p2;
  int ret = strcoll (w1->spelling, w2->spelling);
  if (!ret)
    ret = w1->class > w2->class ? 1 : w1->class < w2->class ? -1 : 0;
  return ret;
}

static int
sel_noun_masculine (const void *data, void *context)
{
  (void) context;
  const struct entry *e = data;
  return e->word.class == NOUN && e->gender == MASCULINE;
}

static void
test3 (void)
{
  puts ("============================================================");
  map *dictionary = map_create (get_word, cmp_word, 0, 0);      // Dictionary. A word can have several definitions and therefore appear several times in the map.
  map_insert_data (dictionary, &(struct entry)
                   {
                   {"Orange", NOUN}, FEMININE, "Fruit"
                   });
  map_insert_data (dictionary, &(struct entry)
                   {
                   {"Abricot", NOUN}, MASCULINE, "Fruit"
                   });
  map_insert_data (dictionary, &(struct entry)
                   {
                   {"Orange", NOUN}, MASCULINE, "Colour"
                   });
  map_insert_data (dictionary, &(struct entry)
                   {
                   {"Orange", ADJECTIVE}, NONE, "Colour"
                   });
  fprintf (stdout, "%lu element(s).\n", map_size (dictionary));
  fprintf (stdout, "%lu element(s).\n", map_traverse (dictionary, 0, 0, 0, 0));
  fprintf (stdout, "%lu element(s) found.\n", map_traverse (dictionary, 0, 0, sel_noun_masculine, 0));
  fprintf (stdout, "%lu element(s) found.\n", map_find_key (dictionary, &(struct word)
                                                            { "Orange", NOUN }, 0, 0));
  fprintf (stdout, "%lu element(s) found.\n", map_find_key (dictionary, &(struct word)
                                                            { "Orange", ADJECTIVE }, 0, 0));
  fprintf (stdout, "%lu element(s) found.\n", map_find_key (dictionary, &(struct word)
                                                            { "Orange", VERB }, 0, 0));
  map_traverse (dictionary, MAP_REMOVE_ALL, 0, 0, 0);
  map_destroy (dictionary);
}

struct crossword
{
  char *word;
  size_t length;
};

static const void *
get_crossword_length (void *data)
{
  struct crossword *cw = data;
  if (!cw->length)
    cw->length = strlen (cw->word);     // The key is calculated from the data and stored in the element.
  return &cw->length;
}

static int
cmp_crossword (const void *p1, const void *p2, void *arg)
{
  (void) arg;
  const size_t l1 = *(const size_t *) p1;
  const size_t l2 = *(const size_t *) p2;
  return l1 > l2 ? 1 : l1 < l2 ? -1 : 0;
}

static int
match (void *data, void *res, int *remove)
{
  (void) remove;
  char *word = ((struct crossword *) data)->word;
  char *pattern = res;
  int match = (strlen (word) == strlen (pattern));
  for (size_t i = 0; match && i < strlen (word); i++)
    if (pattern[i] != '*' && pattern[i] != word[i])
      match = 0;
  if (match)
    fprintf (stdout, "%s\n", word);
  return 1;
}

static void
test4 (void)
{
  char *pattern = "*e***";
  const size_t l = strlen (pattern);
  puts ("============================================================");
  map *dictionary = map_create (get_crossword_length, cmp_crossword, 0, 0);
  map_insert_data (dictionary, &(struct crossword)
                   {.word = "Lemon" });
  map_insert_data (dictionary, &(struct crossword)
                   {.word = "Apple" });
  map_insert_data (dictionary, &(struct crossword)
                   {.word = "Orange" });
  map_insert_data (dictionary, &(struct crossword)
                   {.word = "Apricot" });
  map_insert_data (dictionary, &(struct crossword)
                   {.word = "Peach" });
  map_insert_data (dictionary, &(struct crossword)
                   {.word = "Grapes" });
  map_display (dictionary, stderr, 0);
  fprintf (stdout, "%lu element(s) checked.\n", map_find_key (dictionary, &l, match, pattern));
  fprintf (stdout, "%lu element(s).\n", map_size (dictionary));
  map_traverse (dictionary, MAP_REMOVE_ALL, 0, 0, 0);
  map_destroy (dictionary);
}

static int
select_random (const void *data, void *res)
{
  (void) data;
  (void) res;
  return rand () % 2;
}

static int
sum_squares (void *data, void *op_arg, int *)
{
  int *sum_of_squares = op_arg;
  int *pi = data;
  *sum_of_squares += *pi * *pi;
  return 1;
}

static void
test5 (void)
{
  static const int NB = 100;
  puts ("============================================================");
  map *ints = map_create (0, cmpip, 0, 0);
  for (size_t i = 0; i < (size_t) NB; i++)
  {
    int *pi = malloc (sizeof (*pi));
    *pi = rand () % NB;
    map_insert_data (ints, pi);
  }
  map_display (ints, stderr, toint);
  map_traverse (ints, print_pi, 0, 0, 0);
  fprintf (stdout, "\n");

  int sum_of_squares = 0;
  map_traverse (ints, sum_squares, &sum_of_squares, 0, 0);
  fprintf (stdout, "%i\n", sum_of_squares);
  map_display (ints, stderr, toint);
  map_traverse (ints, print_pi, 0, 0, 0);
  fprintf (stdout, "\n");
  map_traverse (ints, print_pi, 0, select_random, 0);
  fprintf (stdout, "\n");

  map_traverse (ints, MAP_REMOVE_ALL, free, select_random, 0);
  map_display (ints, stderr, toint);
  map_traverse (ints, print_pi, 0, 0, 0);
  fprintf (stdout, "\n");

  map_traverse (ints, MAP_REMOVE_ALL, free, 0, 0);
  map_destroy (ints);
}

static void
test6 (void)
{
#undef map_create
#undef map_destroy
#undef map_insert_data
#undef map_traverse
#undef map_traverse_backward
#undef map_size
#undef map_height
  static const size_t NB = 10 * 1000 * 1000;
  puts ("============================================================");
  struct timespec ts0, ts;
  timespec_get (&ts0, TIME_UTC);
  fprintf (stdout, "Create map...\n");
  map *ints = map_create (0, cmpip, 0, 0);
  timespec_get (&ts, TIME_UTC);
  fprintf (stdout, "[%.Lf ms] %'zu element(s), height %zu.\n", 1000.L * difftime (ts.tv_sec, ts0.tv_sec) + (ts.tv_nsec - ts0.tv_nsec) / 1000000.L,
           map_size (ints), map_height (ints));
  fprintf (stdout, "Insert %'zu sorted elements...\n", NB);
  for (size_t i = 0; i < NB; i++)
  {
    int *pi = malloc (sizeof (*pi));
    *pi = (int) i;
    map_insert_data (ints, pi);
  }
  timespec_get (&ts, TIME_UTC);
  fprintf (stdout, "[%.Lf ms] %'zu element(s), height %zu.\n", 1000.L * difftime (ts.tv_sec, ts0.tv_sec) + (ts.tv_nsec - ts0.tv_nsec) / 1000000.L,
           map_size (ints), map_height (ints));
  fprintf (stdout, "Traverse map...\n");
  int sum_of_squares = 0;
  map_traverse (ints, sum_squares, &sum_of_squares, 0, 0);
  timespec_get (&ts, TIME_UTC);
  fprintf (stdout, "[%.Lf ms] %'zu element(s), height %zu.\n", 1000.L * difftime (ts.tv_sec, ts0.tv_sec) + (ts.tv_nsec - ts0.tv_nsec) / 1000000.L,
           map_size (ints), map_height (ints));
  fprintf (stdout, "Remove the first %'zu elements, one by one...\n", NB / 2);
  for (size_t i = 0; i < NB / 2; i++)
  {
    int *pi;
    map_traverse (ints, MAP_REMOVE_ONE, &pi, 0, 0);
    free (pi);
  }
  timespec_get (&ts, TIME_UTC);
  fprintf (stdout, "[%.Lf ms] %'zu element(s), height %zu.\n", 1000.L * difftime (ts.tv_sec, ts0.tv_sec) + (ts.tv_nsec - ts0.tv_nsec) / 1000000.L,
           map_size (ints), map_height (ints));
  fprintf (stdout, "Remove all remaining %'zu elements...\n", NB - NB / 2);
  map_traverse (ints, MAP_REMOVE_ALL, free, 0, 0);
  timespec_get (&ts, TIME_UTC);
  fprintf (stdout, "[%.Lf ms] %'zu element(s), height %zu.\n", 1000.L * difftime (ts.tv_sec, ts0.tv_sec) + (ts.tv_nsec - ts0.tv_nsec) / 1000000.L,
           map_size (ints), map_height (ints));
  fprintf (stdout, "Destroy empty map...\n");
  map_destroy (ints);
}

int
main (void)
{
  setlocale (LC_ALL, "");
  srand ((unsigned int) time (0));
  test1 ();
  test2 ();
  test3 ();
  test4 ();
  test5 ();
  test6 ();
}
