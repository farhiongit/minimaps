// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#include <threads.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "map.h"

struct map_elem
{
  struct map_elem *upper, *lt, *eq, *gt;
  struct map_elem *previous, *next;
  void *data;
  const void *key_from_data;
  struct map *map;
};

struct map
{
  struct map_elem *first, *last, *root;
  mtx_t mutex;
  map_key_comparator cmp_key;
  map_key_extractor get_key;
  void *arg;
  int uniqueness;               // Properties.
  size_t nb_elem;
};

static const void *
_MAP_KEY_IS_DATA (void *data)
{
  return data;
}

struct map *
map_create (map_key_extractor get_key, map_key_comparator cmp_key, void *arg, int unicity)
{
  if (!get_key && cmp_key)
    get_key = _MAP_KEY_IS_DATA;
  if ((unicity || get_key) && !cmp_key)
  {
    errno = EPERM;
    fprintf (stderr, "%s: %s\n", __func__, "Undefined key comparator.");
    return 0;
  }
  struct map *l = calloc (1, sizeof (*l));
  if (!l)
  {
    errno = ENOMEM;
    fprintf (stderr, "%s: %s\n", __func__, "Out of memory.");
    return 0;
  }
  l->get_key = get_key;
  l->cmp_key = cmp_key;
  l->uniqueness = unicity;
  l->arg = arg;
  // mtx_recursive : the SAME thread can lock (and unlock) the mutex several times. See https://en.wikipedia.org/wiki/Reentrant_mutex for more.
  // Therefore, map_find_key, map_traverse, map_traverse_backward and map_insert_data can call each other.
  if (mtx_init (&l->mutex, mtx_recursive) != thrd_success)
  {
    free (l);
    errno = ENOMEM;
    fprintf (stderr, "%s: %s\n", __func__, "Out of memory.");
    return 0;
  }
  return l;
}

int
map_destroy (struct map *l)
{
  if (!l)
  {
    errno = EINVAL;
    return 0;
  }
  mtx_lock (&l->mutex);
  if (l->first)
  {
    errno = EPERM;
    fprintf (stderr, "%s: %s\n", __func__, "Not empty. Not destroyed.");
    mtx_unlock (&l->mutex);
    return 0;
  }
  mtx_unlock (&l->mutex);
  mtx_destroy (&l->mutex);
  free (l);
  return 1;
}

size_t
map_size (map *m)
{
  mtx_lock (&m->mutex);
  size_t ret = m->nb_elem;
  mtx_unlock (&m->mutex);
  return ret;
}

static struct map_elem *
_map_previous (struct map_elem *e)
{
  struct map_elem *ret = e;
  if (ret->upper && ret == ret->upper->eq)
    return ret->upper;
  if (ret->lt)
    for (ret = ret->lt; ret->gt; ret = ret->gt) /* nothing */ ;
  else if (ret->upper)
  {
    for (; ret && ret->upper && ret == ret->upper->lt; ret = ret->upper) /* nothing */ ;
    if (ret)
      ret = ret->upper;
  }
  else
    ret = 0;
  if (ret)
    for (; ret->eq; ret = ret->eq) /* nothing */ ;
  return ret;
}

static struct map_elem *
_map_next (struct map_elem *e)
{
  struct map_elem *ret = e;
  if (ret->eq)
    return ret->eq;
  for (; ret && ret->upper && ret == ret->upper->eq; ret = ret->upper) /* nothing */ ;  // Go to the root of equal elements
  if (ret->gt)
    for (ret = ret->gt; ret->lt; ret = ret->lt) /* nothing */ ;
  else if (ret->upper)
  {
    for (; ret && ret->upper && ret == ret->upper->gt; ret = ret->upper) /* nothing */ ;
    if (ret)
      ret = ret->upper;
  }
  else
    ret = 0;
  return ret;
}

#define fmapf(stream, ...) ((stream) ? (fprintf ((stream), __VA_ARGS__) + (fflush (stream), (int) 0)): (int) 0)
static void
_map_scan_and_display (struct map_elem *root, FILE *stream, size_t indent, char b, void (*displayer) (FILE *stream, const void *data))
{
  if (root)
  {
    struct map *m = root->map;
    for (size_t i = 0; i < indent; i++)
      fmapf (stream, ". ");
    fmapf (stream, "%c ", b);
    assert (root == m->first ? root->previous == 0 : root->previous != 0);
    assert (root == m->last ? root->next == 0 : root->next != 0);
    assert (root->previous == _map_previous (root));
    assert (root->next == _map_next (root));
    assert (!root->previous || root->previous->next == root);
    assert (!root->next || root->next->previous == root);
    fmapf (stream, "%p ", root);
    if (displayer && stream && root->data)
    {
      fmapf (stream, "[= ");
      displayer (stream, root->data);
      fmapf (stream, "] ");
    }
    else
      fmapf (stream, "[= *%p] ", root->data);
    fmapf (stream, "%s%s", root == m->first ? "(f) " : "", root == m->last ? "(l) " : "");
    for (struct map_elem * eq = root->eq; eq; eq = eq->eq)
    {
      assert (!eq->lt && !eq->gt);
      assert (eq->upper && eq->upper->eq == eq && (!eq->eq || eq->eq->upper == eq));
      assert (eq == m->first ? eq->previous == 0 : eq->previous != 0);
      assert (eq == m->last ? eq->next == 0 : eq->next != 0);
      assert (eq->previous == _map_previous (eq));
      assert (eq->next == _map_next (eq));
      assert (!eq->previous || eq->previous->next == eq);
      assert (!eq->next || eq->next->previous == eq);
      fmapf (stream, "=%s %p ", (m->cmp_key && !m->cmp_key (root->key_from_data, eq->key_from_data, 0)) ? "=" : "?", eq);
      if (displayer && stream && eq->data)
      {
        fmapf (stream, "[= ");
        displayer (stream, eq->data);
        fmapf (stream, "] ");
      }
      else
        fmapf (stream, "[= *%p] ", eq->data);
      fmapf (stream, "%s%s", eq == m->first ? "(f) " : "", eq == m->last ? "(l) " : "");
    }
    fmapf (stream, "\n");
    assert (!root->gt || root->gt->upper == root);
    _map_scan_and_display (root->gt, stream, indent + 1, '<', displayer);
    assert (!root->lt || root->lt->upper == root);
    _map_scan_and_display (root->lt, stream, indent + 1, '>', displayer);
  }
}

struct map *
map_display (struct map *m, FILE *stream, void (*displayer) (FILE *stream, const void *data))
{
  mtx_lock (&m->mutex);
  if (m->root)
  {
    _map_scan_and_display (m->root, stream, 0, '*', displayer);
    assert (!m->root->upper);
    assert (m->nb_elem);
    assert (m->first);
    assert (m->last);
    assert (!m->root->upper && m->nb_elem && m->first && m->last);
    assert (!m->first->lt);
    assert (!m->first->upper || !m->first->upper->eq || (m->first->upper->eq != m->first));
    struct map_elem *e;
    for (e = m->last; e->upper && e->upper->eq == e; e = e->upper);
    assert (!e->gt);
  }
  else
    assert (!m->nb_elem && !m->first && !m->last);
  mtx_unlock (&m->mutex);
  return m;
}

int
map_insert_data (struct map *l, void *data)
{
  if (!l)
  {
    errno = EINVAL;
    return 0;
  }
  struct map_elem *new = calloc (1, sizeof (*new));     // All elements are set to 0.
  if (!new)
  {
    errno = ENOMEM;
    fprintf (stderr, "%s: %s\n", __func__, "Out of memory.");
    return 0;
  }
  new->data = data;
  new->map = l;
  mtx_lock (&l->mutex);
  new->key_from_data = l->get_key ? l->get_key (new->data) : 0; // The key is evaluated only once, at insertion.
  struct map_elem *iter;
  int cmp;
  if (!(iter = l->root))
    l->root = l->first = l->last = new;
  else if (!l->cmp_key)
  {
    (l->last->gt = new)->upper = l->last;
    l->last = new;
  }
  else
    while (1)
      if ((cmp = l->cmp_key ? l->cmp_key (new->key_from_data, iter->key_from_data, l->arg) : 0) < 0)
      {
        if (iter->lt)
          iter = iter->lt;
        else
        {
          if (((iter->lt = new)->upper = iter) == l->first)
            l->first = new;
          break;
        }
      }
      else if (l->uniqueness && cmp == 0)
      {
        fprintf (stderr, "%s: %s\n", __func__, "Already exists. Not inserted.");
        errno = EPERM;
        free (new);             // new is not inserted.
        new = 0;
        break;
      }
      else if (cmp == 0)        // && !l->uniqueness
      {
        for (; iter->eq; iter = iter->eq) /* nothing */ ;
        if (((iter->eq = new)->upper = iter) == l->last)
          l->last = new;
        break;
      }
      else if (iter->gt)        // && cmp > 0
        iter = iter->gt;
      else                      // (!iter->gt) && (cmp > 0)
      {
        struct map_elem *last;
        for (last = l->last; last->upper && last->upper->eq == last; last = last->upper);
        if (((iter->gt = new)->upper = iter) == last)
          l->last = new;
        break;
      }
  if (new)
  {
    if ((new->next = _map_next (new)))
      new->next->previous = new;
    if ((new->previous = _map_previous (new)))
      new->previous->next = new;
    l->nb_elem++;
  }
  mtx_unlock (&l->mutex);
  return new ? 1 : 0;
}

static void *
_map_remove (struct map_elem *old)
{
  struct map_elem *e = old;
  struct map *l = e->map;
  void *data = e->data;

  if (e->previous)
    e->previous->next = e->next;
  if (e->next)
    e->next->previous = e->previous;

  if (l->first == e)
    l->first = e->next;
  if (l->last == e)
    l->last = e->previous;

  if (e->upper && e->upper->eq == e)
  {
    if (e->eq)
      e->eq->upper = e->upper;
    e->upper->eq = e->eq;
  }
  else if (e->eq)
  {
    if (!e->upper)
      l->root = e->eq;
    else if (e->upper->lt == e)
      e->upper->lt = e->eq;
    else if (e->upper->gt == e)
      e->upper->gt = e->eq;
    if (e->lt)
      e->lt->upper = e->eq;
    if (e->gt)
      e->gt->upper = e->eq;
    e->eq->lt = e->lt;
    e->eq->gt = e->gt;
    e->eq->upper = e->upper;
  }
  else
  {
    if (e->lt && e->gt)
    {
      if (rand () & 1)
      {
        struct map_elem *lt = e->lt;
        e->lt = 0;
        for (e = e->gt; e->lt; e = e->lt) /* nothing */ ;
        (e->lt = lt)->upper = e;
      }
      else
      {
        struct map_elem *gt = e->gt;
        e->gt = 0;
        for (e = e->lt; e->gt; e = e->gt) /* nothing */ ;
        (e->gt = gt)->upper = e;
      }
      e = old;
    }

    struct map_elem *child;
    if ((child = (e->lt ? e->lt : e->gt)))
      child->upper = e->upper;
    if (!e->upper)
      l->root = child;          // Update map->root
    else if (e == e->upper->lt)
      e->upper->lt = child;
    else                        // (e == e->upper->gt)
      e->upper->gt = child;
  }

  free (e);
  l->nb_elem--;
  return data;
}

static size_t
_map_traverse (map *m, map_operator op, map_selector sel, void *context, int backward)
{
  if (!m)
  {
    errno = EINVAL;
    return 0;
  }
  mtx_lock (&m->mutex);
  //_map_scan_and_display (m->root, stderr, 0, '*');
  size_t nb_op = 0;
  for (struct map_elem * e = backward ? m->last : m->first; e;)
  {
    int remove = 0;
    int go_on = 1;
    if (!sel || sel (e->data, context))
    {
      if (op)
        go_on = op (e->data, context, &remove);
      nb_op++;
    }
    struct map_elem *n = backward ? e->previous : e->next;
    if (remove)
      _map_remove (e);
    if (!go_on)
      break;
    e = n;
  }
  //_map_scan_and_display (m->root, stderr, 0, '*');
  mtx_unlock (&m->mutex);
  return nb_op;
}

size_t
map_traverse (map *m, map_operator op, map_selector sel, void *context)
{
  return _map_traverse (m, op, sel, context, 0);
}

size_t
map_traverse_backward (map *m, map_operator op, map_selector sel, void *context)
{
  return _map_traverse (m, op, sel, context, 1);
}

size_t
map_find_key (struct map *l, const void *key, map_operator op, void *context)
{
  if (!l || !key)
  {
    errno = EINVAL;
    return 0;
  }
  if (!l->cmp_key)
  {
    fprintf (stderr, "%s: %s\n", __func__, "Undefined key comparator.");
    errno = EPERM;
    return 0;
  }
  mtx_lock (&l->mutex);
  size_t nb_op = 0;
  int cmp_key;
  struct map_elem *iter = l->root;
  while (iter)
    if ((cmp_key = l->cmp_key (key, iter->key_from_data, l->arg)) < 0)
      iter = iter->lt;
    else if (cmp_key == 0)
    {
      int remove = 0;
      int go_on = op ? op (iter->data, context, &remove) : 1;
      struct map_elem *next = iter->eq; // After op is called. An added equal element while finding will be found later.
      nb_op++;
      if (remove)
        _map_remove (iter);
      iter = go_on ? next : 0;
    }
    else                        // cmp_key > 0
      iter = iter->gt;
  mtx_unlock (&l->mutex);
  return nb_op;
}

static int
_MAP_REMOVE (void *data, void *context, int *remove)
{
  if (context)
    *(void **) context = data;  // *context is supposed to be a pointer here.
  *remove = 1;
  return 0;
}

map_operator MAP_REMOVE_ONE = _MAP_REMOVE;

static int
_MAP_REMOVE_ALL (void *data, void *context, int *remove)
{
  if (context)
  {
    void (*dtor) (void *) = context;
    dtor (data);                // *context is supposed to be a pointer to a destructor here.
  }
  *remove = 1;
  return 1;
}

map_operator MAP_REMOVE_ALL = _MAP_REMOVE_ALL;

static int
_MAP_MOVE (void *data, void *context, int *remove)
{
  if (!context)
  {
    fprintf (stderr, "%s: %s\n", "MAP_MOVE", "Context must not be a null pointer.");
    errno = EINVAL;
    return 0;
  }
  // if context is the same as the map containing data, i.e. (map *)context is already locked, ...
  return (*remove = map_insert_data (context, data));   // *context is supposed to be a pointer to a map here.
}

map_operator MAP_MOVE_TO = _MAP_MOVE;
