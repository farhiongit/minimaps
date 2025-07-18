/********************** # Map me ! *********************/
// **A unprecedented MT-safe implementation of a map library that can manage maps, sets, sorted and unsorted lists and that can do it all with a minimalist interface.**
//
// (c) L. Farhi, 2024.
// Language: C (C11 or higher).

/* This library manages sorted maps, sorted sets, sorted and unsorted lists, FIFO and LIFO queues (depending on how the "map" is created).
   The interface has only 7 functions to do everything (create, read, update, insert, remove, destroy):

- `map_create`
- `map_destroy`
- `map_size` (MT-safe)
- `map_insert_data` (MT-safe)
- `map_find_key` (MT-safe)
- `map_traverse` (MT-safe)
- `map_traverse_backward` (MT-safe)

They are detailed below.
> All calls are MT-safe: concurrent threads using the same "map" will synchronise (block and wait for each other).
> All calls are non-recursive.
*/

// ## Type definitions
#ifndef  __MAP_H__
#  define __MAP_H__

// ### Map
// A map as an opaque Abstract Data Type (internally modelled as a sorted binary tree):
typedef struct map map;
/* The map stores pointers to allocated data:

  void *data;
*/

// ### Key
// The key of the map is extracted from the data stored in it (generally but not necessarily a subset of it). A user-defined function of type `map_key_extractor` (passed to `map_create`) can be used to extract this subset.
// `map_key_extractor` is the type of the user-defined function that should return a pointer to the the part of `data` that contains the key of the map.
typedef const void *(*map_key_extractor) (void *data);
// > Functions of type `map_key_extractor` should not allocate memory dynamically.
/* Example:

  enum class { NOUN, VERB, ADJECTIVE, ADVERB, PRONOUN, DETERMINER, PREPOSITION, CONJUNCTION, INTERJECTION };
  enum gender { MASCULINE, FEMININE, NEUTER };
  struct entry  // The type of the data stored in the map
  {
    struct word { char *spelling ; enum class class ; } word;
    enum gender gender ;
    char* definition;
  };

  static const void* get_word (const void* data)       // 'data' is supposed to be a pointer to 'struct entry'
  {
    return &((const struct entry *)data)->word;        // 'word' is declared as the subset of the 'data' that defines the key of the map.
  }

*/

// ### Key comparator
// The type of a user-defined function that compares two keys of elements of a map.
typedef int (*map_key_comparator) (const void *key_a, const void *key_b, void *arg);
// `key_a` and `key_b` are pointers to keys, as they would be returned by a function of type `map_key_extractor`.
// A comparison function must return an integer less than, equal to, or greater than zero if the first argument is considered to be respectively less than, equal to, or greater than the second.
// The third argument `arg` receives the pointer that was passed to `map_create`.
/* Example:

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

*/

// ### Selector on elements of the map
// The type of a user-defined function that selects elements while traversing a map with `map_traverse` or `map_traverse_backward`. 
typedef int (*map_selector) (const void *data, void *sel_arg);
// The data of the element of the map is passed as the first argument of the map_selector.
// The second argument `sel_arg` receives the pointer passed to `map_traverse` and `map_traverse_backward`.
// Should return `1` if the `data` conforms to the user-defined conditions (and should be selected by `map_traverse` or `map_traverse_backward`), `0` otherwise.

// ### Operator on elements of the map
// The type of a user-defined function that operates on (accesses, and optionally modifies or removes) an element of a map
// picked by `map_traverse`, `map_traverse_backward` or `map_find_key`.
typedef int (*map_operator) (void *data, void *op_arg, int *remove);
// The data of the element of the map is passed as the first argument of the `map_operator`.
// The second argument `op_arg` receives the pointer passed to `map_traverse`, `map_traverse_backward` and `map_find_key`.
// The third argument `remove` receives a non-null pointer for which `*remove` is set to `0`.
// If (and only if) the operator sets `*remove` to a non-zero value,
//
//   - the element will be removed from the map thread-safely ;
//   - the operator **should** keep track and ultimately free the data passed to it if it was allocated dynamically (otherwise data would be lost in memory leaks).
// The `map_operator` should return `1` if the operator should be applied on further elements of the map, `0` otherwise. In other words,
// as soon as the operator returns `0`, it stops `map_traverse`, `map_traverse_backward` or `map_find_key`. 
// > The operator `map_operator` should neither modify the pointer returned by `map_key_extractor` nor its content (as evaluated by `map_key_comparator`). In other words, the key of the element in the map should remain untouched by `map_operator`, otherwise results are undefined.

// ## Interface

// ### Create a map
map *map_create (map_key_extractor get_key, map_key_comparator cmp_key, void *arg, int unicity);
// Returns `0` if the map could not be allocated (and `errno` set to `ENOMEM`).
// Otherwise, returns a pointer to the created map.
// If not `0`, the comparison function `cmp_key` must return an integer less than, equal to, or greater than zero
// if the first argument is considered to be respectively less than, equal to, or greater than the second.
// `cmp_key` is applied on `get_key (data)` if `get_key` is not `0` ; otherwise, `cmp_key` is applied on `data` (where `data` is a pointer inserted by `map_insert_data`).
// `cmp_key` must be set if `get_key` is set.
// The pointer `arg` (which can be `0`) is passed to the comparison function `cmp_key` (as third argument).

// If `unicity` is not `0`, elements are unique in the map (equal elements are not inserted and `map_insert_data` will return `0`).
// Otherwise, equal elements are sorted in the order they were inserted.

/* 7 possible uses, depending on `property`, `cmp_key` and `get_key`:

| Use            | `unicity` | `cmp_key` | `get_key` | Comment                                                                                                                       |
| -              | -         | -         | -         | -                                                                                                                             |
| Sorted map     | `1`       | Non-zero  | Non-zero  | Each key is unique in the map.                                                                                                |
| Dictionary     | `0`       | Non-zero  | Non-zero  | Keys can have multiple entries in the map.                                                                                    |
| Sorted set     | `1`       | Non-zero  | `0`       | Elements are unique. `cmp_key` applies to inserted `data` (the `data` is the key).                                            |
| Sorted list    | `0`       | Non-zero  | `0`       | Equal elements are sorted in the order they were inserted. `cmp_key` applies to inserted `data` (the `data` is the key).      |
| Unsorted list  | `0`       | `0`       | `0`       |                                                                                                                               |
| FIFO           | `0`       | `0`       | `0`       | Elements are appended after the last element. Use `map_traverse (m, MAP_REMOVE_ONE, 0, &data)` to remove an element.          |
| LIFO           | `0`       | `0`       | `0`       | Elements are appended after the last element. Use `map_traverse_backward (m, MAP_REMOVE_ONE, 0, &data)` to remove an element. |

> (*) If `cmp_key` or `get_key` is `0`, complexity is reduced by a factor log n.

*/

// ### Destroy a map
int map_destroy (map *);
// Destroys an **empty** and previously created map.
// If the map is not empty, the map is not destroyed.
// Returns `0` (and `errno` set to `EPERM`) if the map is not empty (and the map is NOT destroyed), `1` otherwise.

// ### Retrieve the number of elements in a map
size_t map_size (map *);
// Returns the number of elements in a map.
// Note: if the map is used by several threads, `map_size` should better not be used since the size of the map can be modified any time by other threads.
// Complexity : 1. MT-safe.

// ### Add an element into a map
int map_insert_data (map *, void *data);
// Adds a previously allocated data into map and returns `1` if the element was added, `0` otherwise.
// Complexity : log n (see (*) above). MT-safe. Non-recursive.

// ### Retrieve and remove elements from a map

// #### Find an element from its key
size_t map_find_key (struct map *map, const void *key, map_operator op, void *op_arg);
// If `get_key` is not null, applies `op` on the data of the elements in the map that matches the key (for which `cmp_key (get_key (data))` returns `0`), as long as `op` returns non-zero.
// If `get_key` is null, applies `operator` on the data of the elements in the map that matches the data (for which `cmp_key (data)` returns `0`), as long as `op` returns non-zero.
// If `op` is null, all the matching elements are found (and counted).
// `op_arg` is passed as the second argument of operator `op`.
// Returns the number of elements on which the operator `op` has been applied.
// Complexity : log n (see (*)). MT-safe. Non-recursive.
// > `cmp_key` should have been previously set by `map_create`.
// > If `op` is null, `map_find_key` simply counts and returns the number of matching elements with the `key`.
// > `map_find_key`, `map_traverse`, `map_traverse_backward` and `map_insert_data` can call each other *in the same thread* (the first argument `map` can be passed again through the `op_arg` argument). Therefore,
// elements can be removed from (when `*remove` is set to `1` in `op`) or inserted into (when `map_insert_data` is called in `op`) the map *by the same thread* while finding elements.

// #### Traverse a map
size_t map_traverse (map * map, map_operator op, void *op_arg, map_selector sel, void *sel_arg);
size_t map_traverse_backward (map * map, map_operator op, void *op_arg, map_selector sel, void *sel_arg);
// Traverse the elements of the map.
// If the operator `op` is not null, it is applied on the data stored in the map, from the first element to the last (resp. the other way round), as long as the operator `op` returns non-zero.
// If `op` is null, all the elements are traversed.
// If the selector `sel` is not null, elements for which `sel (data)` (where `data` is an element previously inserted into the map) returns `0` are ignored. `map_traverse` (resp.`map_traverse_backward`) behaves as if the operator `op` would start with: `if (!sel (data, sel_arg)) return 1;`.
// `op_arg` and `sel_arg` are respectively passed as the second argument of operator `op` and selector `sel`.
// Returns the number of elements of the map that match `sel` (if set) and on which the operator `op` (if set) has been applied.
// Complexity : n. MT-safe. Non-recursive.
// > If `op` is null, `map_traverse` and `map_traverse_backward` simply count and return the number of matching elements with the selector `sel` (if set). If `op` and `sel `are null, `map_traverse` and `map_traverse_backward` simply count and return the number of elements.
// > `map_find_key`, `map_traverse`, `map_traverse_backward` and `map_insert_data` can call each other *in the same thread* (the first argument `map` can be passed again through the `op_arg` argument). Therefore,
// elements can be removed from (when `*remove` is set to `1` in `op`) or inserted into (when `map_insert_data` is called in `op`) the map *by the same thread* while traversing elements.
// > Insertion while traversing should be done with care since an infinite loop will occur if, in `op`, an element is removed and :
// >
// >  - while traversing forward, at least an equal or greater element is inserted ;
// >  - while traversing backward, at least a lower element is inserted.

// ### Predefined operators for use with `map_find_key`, `map_traverse` and `map_traverse_backward`.

// `map_operator` functions passed to `map_find_key`, `map_traverse` and `map_traverse_backward` should be user-defined.
// But useful operators are provided below.

// #### Map operator to retrieve one element
// This map operator simply retrieves one element from the map.
extern map_operator MAP_GET_ONE;
// > Its use is **not recommended** though. Actions on an element should better be directly integrated in the `map_operator` function.
// The helper operator `MAP_GET_ONE` retrieves an element found by `map_find_key`, `map_traverse` or `map_traverse_backward`
// and, if the parameter `op_arg` of `map_find_key`, `map_traverse` or `map_traverse_backward` is a non null pointer,
// it sets the pointer `op_arg` to the data of this element.
// `op_arg` **should be** the address of a pointer to type T, where `op_arg` is the argument passed to `map_find_key`, `map_traverse` or `map_traverse_backward`.
// Example: to get the last element, use `T *data = 0; if (map_traverse_backward (m, MAP_GET_ONE, 0, &data)) { ... }`

// #### Map operator to retrieve and remove one element
// This map operator simply retrieves and removes one element from the map.
extern map_operator MAP_REMOVE_ONE;
// > Its use is **not recommended** though. Actions on an element should better be directly integrated in the `map_operator` function.
// The helper operator `MAP_REMOVE_ONE` removes and retrieves an element found by `map_find_key`, `map_traverse` or `map_traverse_backward`
// and, if the parameter `op_arg` of `map_find_key`, `map_traverse` or `map_traverse_backward` is a non null pointer,
// it sets the pointer `op_arg` to the data of this element.
// `op_arg` **should be** `0` or the address of a pointer to type T, where `op_arg` is the argument passed to `map_find_key`, `map_traverse` or `map_traverse_backward`.
/* Example

If `m` is a map of elements of type T and `sel` a map_selector, the following piece of code will remove and retrieve the data of the first element selected by `sel`:

  T *data = 0;  // `data` is a *pointer* to the type stored in the map.
  if (map_traverse (m, MAP_REMOVE_ONE, sel, &data) && data)  // A *pointer to the pointer* `data` is passed to map_traverse.
  {
    // `data` can thread-safely be used to work with.
    ...
    // If needed, it can be reinserted in the map after use.
    map_insert_data (m, data);
  }
*/

// #### Map operator to remove all elements
// This map operator removes all the element from the map.
extern map_operator MAP_REMOVE_ALL;
// the parameter `op_arg` of `map_find_key`, `map_traverse` or `map_traverse_backward` should be `0` or a pointer to a destructor function with signature `void (*)(void * ptr)` (such as `free`).
// This destructor is applied to each element selected by `map_find_key`, `map_traverse` or `map_traverse_backward`.

// #### Map operator to move elements from one map to another
// This map operator moves each element selected by `map_find_key`, `map_traverse` or `map_traverse_backward` to another **different** map passed in the argument `op_arg` of `map_find_key`, `map_traverse` or `map_traverse_backward`.
// N.B.: A destination map identical to the source map would **deadly lock** the calling thread.
extern map_operator MAP_MOVE_TO;

// ## For debugging purpose
#  include <stdio.h>
#  define map_check(map) map_display ((map), 0, 0)
struct map *map_display (map * map, FILE * stream, void (*displayer) (FILE * stream, const void *data));
// For fans only.

#endif
