// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#undef NDEBUG
#include <threads.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "map.h"

struct map_elem
{
  struct map_elem *upper /* parent */ , *lt /* less than */ , *gt /* greater than */ ;  // Binary tree structure
  struct map_elem *eq_next /* next equal element */ , *eq_head /* head of equal elements */ , *eq_tail /* tail of equal elements */ ;   // List of equal elements
  struct map_elem *previous_lt, *next_gt;       // Double-linked list structure
  void *data;
  const void *key_from_data;
  struct map *map;              // Owner
  size_t height;                // Distance to the bottom of the tree
};

struct map
{
  struct map_elem *first, *last, *root;
  mtx_t mutex;
  map_key_comparator cmp_key;
  map_key_extractor get_key;
  void *arg;
  int uniqueness;               // Property
  size_t nb_balancing;
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
  if (mtx_init (&l->mutex, mtx_plain | mtx_recursive) != thrd_success)
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

size_t
map_height (map *m)
{
  mtx_lock (&m->mutex);
  size_t ret = m->root ? m->root->height : 0;
  mtx_unlock (&m->mutex);
  return ret;
}

size_t
map_nb_balancing (map *m)
{
  mtx_lock (&m->mutex);
  size_t ret = m->nb_balancing;
  mtx_unlock (&m->mutex);
  return ret;
}

static struct map_elem *
_map_previous_lt (struct map_elem *e)
{
  struct map_elem *ret = e;
  if (ret->lt)
    for (ret = ret->lt; ret->gt; ret = ret->gt) /* nothing */ ; // lowest below e
  else if (ret->upper)
  {
    for (; ret && ret->upper && ret == ret->upper->lt; ret = ret->upper) /* nothing */ ;
    ret = ret->upper;
  }
  else
    ret = 0;
  return ret;
}

static struct map_elem *
_map_previous (struct map_elem *e)
{
  struct map_elem *ret = e;
  if (ret->upper && ret == ret->upper->eq_next)
    return ret->upper;
  ret = ret->previous_lt;
  if (ret && ret->eq_next)
    ret = ret->eq_tail;         // Go to the bottom of equal elements
  return ret;
}

static struct map_elem *
_map_next_gt (struct map_elem *e)
{
  struct map_elem *ret = e;
  if (ret->gt)
    for (ret = ret->gt; ret->lt; ret = ret->lt) /* nothing */ ; // highest below e
  else if (ret->upper)
  {
    for (; ret && ret->upper && ret == ret->upper->gt; ret = ret->upper) /* nothing */ ;
    ret = ret->upper;
  }
  else
    ret = 0;
  return ret;
}

static struct map_elem *
_map_next (struct map_elem *e)
{
  struct map_elem *ret = e;
  if (ret->eq_next)
    return ret->eq_next;
  if (ret->eq_head)
    ret = ret->eq_head;         // Go to the top of equal elements
  return ret->next_gt;
}

// _map_get_high MUST be called on a node every time one of its children (e->lt our e->gt) is modified.
static void
_map_get_high (struct map_elem *from)
{
  for (struct map_elem * e = from; e; e = e->upper)
  {
    size_t h = e->height;
    (void) h;
    if (!e->lt && !e->gt)
      e->height = 1;            // > 0
    else if (!e->lt)
      e->height = e->gt->height + 1;
    else if (!e->gt)
      e->height = e->lt->height + 1;
    else if (e->lt->height < e->gt->height)
      e->height = e->gt->height + 1;
    else
      e->height = e->lt->height + 1;
    if (e->height == h)
      break;
  }
}

static int
_map_fold (struct map_elem *A)
{
/*
     A
      \
       B
      /
     C     =>   C
               / \
              A   B
*/
  if (!A || A->height != 3 || (A->lt && A->gt) || (!A->lt && !A->gt))
    return 0;
  struct map_elem *B = A->lt ? A->lt : A->gt;
  if ((B->lt && B->gt) || (!B->lt && !B->gt) || (A->gt && B->gt) || (A->lt && B->lt))
    return 0;
  struct map_elem *C = B->lt ? B->lt : B->gt;
  if (C->lt || C->gt)
    return 0;
  struct map_elem *P = A->upper;
  C->gt = A->gt ? A->gt : A;
  C->lt = A->lt ? A->lt : A;
  assert ((C->lt == A && C->gt == B) || (C->gt == A && C->lt == B));
  A->lt = A->gt = B->lt = B->gt = 0;
  A->upper = B->upper = C;
  _map_get_high (A);
  _map_get_high (B);
  _map_get_high (C);
  if ((C->upper = P))
  {
    if (P->lt == A)
      P->lt = C;
    else
      P->gt = C;
    _map_get_high (P);
  }
  else
    A->map->root = C;
  A->map->nb_balancing++;
  return 1;
}

static void
_map_rotate_left (struct map_elem *A)
{
/*
     A    h(A) = max(h(e),max(h(d),h(C))+1)+1
    / \
   e   B
      / \
     C   d =>   B    h(B) = max(h(d),max(h(e),h(C))+1)+1
               / \
              A   d
             / \
            e   C
*/
  struct map_elem *P = A->upper;
  struct map_elem *B = A->gt;
  if (!B)
    return;
  struct map_elem *C = B->lt;
  if ((A->gt = C))
    C->upper = A;
  B->lt = A;
  A->upper = B;
  if ((B->upper = P))
  {
    if (P->lt == A)
      P->lt = B;
    else
      P->gt = B;
  }
  else
    A->map->root = B;
  // NOTE: the height of B may have not changed but the height of its parent has (for instance if e and C are null).
  _map_get_high (A);
  _map_get_high (B);
  _map_get_high (P);
  A->map->nb_balancing++;
}

static void
_map_rotate_right (struct map_elem *A)
{
  struct map_elem *P = A->upper;
  struct map_elem *B = A->lt;
  if (!B)
    return;
  struct map_elem *C = B->gt;
  if ((A->lt = C))
    C->upper = A;
  B->gt = A;
  A->upper = B;
  if ((B->upper = P))
  {
    if (P->gt == A)
      P->gt = B;
    else
      P->lt = B;
  }
  else
    A->map->root = B;
  _map_get_high (A);
  _map_get_high (B);
  _map_get_high (P);
  A->map->nb_balancing++;
}

static void
_map_balance (struct map_elem *from)
{
  static const size_t balancing_threashold = 1;
  if (!balancing_threashold)
    return;
  for (struct map_elem * e = from; e;)
  {
    struct map_elem *n = e->upper;
    if (!_map_fold (e))
    {
      size_t lh = e->lt ? e->lt->height : 0;
      size_t gh = e->gt ? e->gt->height : 0;
      if (lh > gh + balancing_threashold)
        _map_rotate_right (e);
      else if (gh > lh + balancing_threashold)
        _map_rotate_left (e);
    }
    e = n;
  }
}

void (*const SHAPE) (FILE * stream, const void *data) = (const void *) (&SHAPE);
static void
nop_displayer (FILE *stream, const void *data)
{
  (void) stream;
  (void) data;
}

#define fmapf(stream, ...) ((stream) ? (fprintf ((stream), __VA_ARGS__) + (fflush (stream), (int) 0)): (int) 0)
static void
_map_scan_and_display (struct map_elem *root, FILE *stream, size_t indent, char b, void (*displayer) (FILE *stream, const void *data))
{
  if (!root)
    return;
  if (!displayer)
    displayer = nop_displayer;
  if (displayer == SHAPE)
  {
    _map_scan_and_display (root->lt, stream, indent + 1, 'v', displayer);
    if (indent)
      fmapf (stream, "%c", b);
    for (size_t i = 1; i < indent; i++)
      fmapf (stream, "%c", '-');
    fmapf (stream, "%c\n", '*');
    _map_scan_and_display (root->gt, stream, indent + 1, '^', displayer);
  }
  else
  {
    assert (!root->lt || root->lt->upper == root);
    _map_scan_and_display (root->lt, stream, indent + 1, '>', displayer);
    struct map *m = root->map;
    fmapf (stream, "%3zu:", root->height);
    for (size_t i = 0; i < indent; i++)
      fmapf (stream, ". ");
    assert (root != root->upper && root != root->lt && root != root->gt && root != root->previous_lt && root != root->next_gt && root != root->eq_next);
    assert (!root->upper || root->upper->eq_next != root);
    assert (root == m->first ? root->previous_lt == 0 : root->previous_lt != 0);
    assert (root->previous_lt == _map_previous_lt (root));
    assert (root->next_gt == _map_next_gt (root));
    assert (!root->previous_lt || root->previous_lt->next_gt == root);
    assert (!root->next_gt || root->next_gt->previous_lt == root);
    if (root->data)
    {
      fmapf (stream, "(");
      if (root->upper)
      {
        fmapf (stream, "'");
        displayer (stream, root->upper->data);
        fmapf (stream, "' ");
      }
      fmapf (stream, "%c) '", b);
      displayer (stream, root->data);
      fmapf (stream, "' (in ] ");
      if (root->previous_lt && root->previous_lt->data)
      {
        fmapf (stream, "'");
        displayer (stream, root->previous_lt->data);
        fmapf (stream, "' ");
      }
      fmapf (stream, "; ");
      if (root->next_gt && root->next_gt->data)
      {
        fmapf (stream, "'");
        displayer (stream, root->next_gt->data);
        fmapf (stream, "' ");
      }
      fmapf (stream, "[");
    }
    fmapf (stream, "%s%s", root == m->first ? ", f" : "", root == m->last ? ", l" : "");
    fmapf (stream, ") ");
    assert (!root->eq_next || root != m->last);
    assert (!root->eq_next || (root->eq_tail && !root->eq_tail->eq_next && root->eq_tail->eq_head == root));
    for (struct map_elem * eq = root->eq_next; eq; eq = eq->eq_next)
    {
      assert (!eq->lt && !eq->gt);
      assert (eq->upper && eq->upper->eq_next == eq && (!eq->eq_next || eq->eq_next->upper == eq));
      assert (eq != m->first);
      assert (!eq->eq_next || eq != m->last);
      assert (eq->previous_lt == 0);
      assert (eq->next_gt == 0);
      assert (eq->eq_next || (eq->eq_head == root && eq->eq_head->eq_tail == eq));
      assert (m->cmp_key && !m->cmp_key (root->key_from_data, eq->key_from_data, 0));
      fmapf (stream, "== '");
      displayer (stream, eq->data);
      fmapf (stream, "' (");
      fmapf (stream, "%s%s%s", eq == m->first ? "f" : "", eq == m->first && eq == m->last ? " ," : "", eq == m->last ? "l" : "");
      fmapf (stream, ") ");
    }
    fmapf (stream, "\n");
    assert (!root->gt || root->gt->upper == root);
    _map_scan_and_display (root->gt, stream, indent + 1, '<', displayer);
    assert (root->height);
    assert (root->lt || root->gt || root->height == 1);
    assert (root->lt || !root->gt || root->height == root->gt->height + 1);
    assert (root->gt || !root->lt || root->height == root->lt->height + 1);
    assert (!root->lt || root->height >= root->lt->height + 1);
    assert (!root->gt || root->height >= root->gt->height + 1);
    assert (!root->lt || !root->gt || root->height == root->lt->height + 1 || root->height == root->gt->height + 1);
  }                             // if (root)
}

struct map *
map_display (struct map *m, FILE *stream, void (*displayer) (FILE *stream, const void *data))
{
  fmapf (stream, "%'zu elements [%'zu]:\n", map_size (m), map_nb_balancing (m));
  mtx_lock (&m->mutex);
  if (m->root)
  {
    _map_scan_and_display (m->root, stream, 0, '*', displayer);
    assert (!m->root->upper);
    assert (m->nb_elem);
    assert (m->first);
    assert (m->last);
    assert (!m->last->eq_next);
    assert (!m->root->upper && m->nb_elem && m->first && m->last);
    assert (!m->first->lt);
    assert (!m->first->upper || !m->first->upper->eq_next || (m->first->upper->eq_next != m->first));
    assert (!m->last->eq_head || !m->last->eq_head->gt);
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
  struct map_elem *new = calloc (1, sizeof (*new));     // All attributes are set to 0.
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
  int cmp, is_last;
  is_last = 1;
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
        is_last = 0;
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
        errno = EPERM;
        free (new);             // new is not inserted.
        new = 0;                // invalidate insertion.
        break;
      }
      else if (cmp == 0)        // && !l->uniqueness
      {
        new->eq_head = iter;
        if (iter->eq_next)
          iter = iter->eq_tail; // Insert at the tail
        new->eq_head->eq_tail = new;
        if (((iter->eq_next = new)->upper = iter) == l->last)
          l->last = new;
        l->nb_elem++;
        mtx_unlock (&l->mutex);
        return 1;
      }
      else if (iter->gt)        // && cmp > 0
        iter = iter->gt;
      else                      // (!iter->gt) && (cmp > 0)
      {
        (iter->gt = new)->upper = iter;
        if (is_last)
          l->last = new;
        break;
      }
  if (new)
  {
    if ((new->next_gt = _map_next_gt (new)))
      new->next_gt->previous_lt = new;
    if ((new->previous_lt = _map_previous_lt (new)))
      new->previous_lt->next_gt = new;
    l->nb_elem++;
    _map_get_high (new);
    _map_get_high (iter);
    _map_balance (new);
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

  if (l->first == e)
    l->first = _map_next (e);
  if (l->last == e)
    l->last = _map_previous (e);

  if (e->upper && e->upper->eq_next == e)       // e is not the head of equal elements
  {
    if (e->eq_next)             // e is not the tail of equal elements
      e->eq_next->upper = e->upper;
    else                        // e is the tail of equal elements
    {
      e->eq_head->eq_tail = e->upper;
      e->upper->eq_head = e->eq_head;
    }
    e->upper->eq_next = e->eq_next;
  }
  else if (e->eq_next)          // e is the head of equal elements
  {
    if (e->eq_next->eq_next)    // There are more than 2 equal elements
    {
      e->eq_next->eq_tail = e->eq_tail;
      e->eq_tail->eq_head = e->eq_next;
    }
    else
      e->eq_next->eq_head = e->eq_next->eq_tail = 0;
    if (!e->upper)
      l->root = e->eq_next;
    else if (e->upper->lt == e)
      e->upper->lt = e->eq_next;
    else if (e->upper->gt == e)
      e->upper->gt = e->eq_next;
    if (e->lt)
      e->lt->upper = e->eq_next;
    if (e->gt)
      e->gt->upper = e->eq_next;
    e->eq_next->lt = e->lt;
    e->eq_next->gt = e->gt;
    e->eq_next->upper = e->upper;
    e->eq_next->previous_lt = e->previous_lt;
    e->eq_next->next_gt = e->next_gt;
    if (e->previous_lt)
      e->previous_lt->next_gt = e->eq_next;
    if (e->next_gt)
      e->next_gt->previous_lt = e->eq_next;
    e->eq_next->height = e->height;
  }
  else if (e->lt && e->gt)
  {
    /* Makes use of the method proposed by T. Hibbard in 1962:
       swap the node to be deleted with its successor or predecessor.
       It changes the heights of the subtrees of 'old' by at most one.
       See https://mathcenter.oxford.emory.edu/site/cs171/hibbardDeletion .
       IMPORTANT NOTICE: Extra constraint imposed by from the interface:
       Only the pointer 'old' passed as an argument should be destroyed
       by the caller. Thus it cannot be reused as a placeholder for
       the key and data of its predecessor or successor. Therefore,
       the predecessor or successor will rather be moved in place of 'old. */
    // e->previous_lt && e->next_gt
    struct map_elem *hibbard62 = e->lt->height > e->gt->height ? e->previous_lt : e->next_gt;
    struct map_elem *invalidated = hibbard62->upper == e ?
      /* if hibbard62 is a child of e */ hibbard62 :
      /* if hibbard62 is a grand-child of e */ hibbard62->upper;
    // Here, !hibbard62->lt || !hibbard62->gt
    // Move 'hibbard62' to the place where 'e' is in the tree.
    if (hibbard62 == e->previous_lt)
      (hibbard62->next_gt = e->next_gt)->previous_lt = hibbard62;
    else if (hibbard62 == e->next_gt)
      (hibbard62->previous_lt = e->previous_lt)->next_gt = hibbard62;
    // N.B.: hibbard62 can sometimes be equal to e->lt or e->gt
    struct map_elem *child = hibbard62->lt ? hibbard62->lt : hibbard62->gt;
    // - Remove hibbard62 from the tree: change the 2 links
    //   (from parent and child) pointing to hibbard62.
    // hibbard62->upper != 0
    // One (and only one) child of the parent of hibbard62 is changed.
    // N.B.: e's children are changed here if hibbard62 is a child of e
    if (hibbard62->upper->lt == hibbard62)
      hibbard62->upper->lt = child;
    else if (hibbard62->upper->gt == hibbard62)
      hibbard62->upper->gt = child;
    _map_get_high (hibbard62->upper);
    if (child)
      child->upper = hibbard62->upper;
    // Here, nobody points to hibbard62 anymore.
    // Move 'hibbard62' to the place where 'e' is in the tree.
    // - Changes the 6 links of hibbard for the ones of node
    hibbard62->upper = e->upper;
    hibbard62->lt = e->lt;
    if (e->lt)
      e->lt->upper = hibbard62;
    hibbard62->gt = e->gt;
    _map_get_high (hibbard62);
    if (e->gt)
      e->gt->upper = hibbard62;
    if (e->upper)
    {
      if (e->upper->lt == e)
        e->upper->lt = hibbard62;
      else if (e->upper->gt == e)
        e->upper->gt = hibbard62;
    }
    else
      l->root = hibbard62;
    _map_get_high (e->upper);
    // Here, some nodes point to hibbard62 again.
    // Here, no node points to e anymore.
    // Invalidate the modified node
    _map_balance (invalidated);
  }                             // if (e->lt && e->gt)
  else                          // if (!e->lt || !e->gt)
  {
    if (e->previous_lt)
      e->previous_lt->next_gt = e->next_gt;
    if (e->next_gt)
      e->next_gt->previous_lt = e->previous_lt;
    struct map_elem *child;
    // Here, !e->lt || !e->gt
    if ((child = (e->lt ? e->lt : e->gt)))
      child->upper = e->upper;
    if (!e->upper)
      l->root = child;          // Update map->root
    else if (e == e->upper->lt)
      e->upper->lt = child;
    else if (e == e->upper->gt) // (e == e->upper->gt)
      e->upper->gt = child;
    _map_get_high (e->upper);
    _map_balance (e->upper);    // One (and only one) of the children of the parent has changed.
  }                             // if (!e->lt || !e->gt)
  free (old);
  l->nb_elem--;
  return data;
}

static size_t
_map_traverse (map *m, map_operator op, void *op_arg, map_selector sel, void *sel_arg, int backward)
{
  if (!m)
  {
    errno = EINVAL;
    return 0;
  }
  mtx_lock (&m->mutex);
  size_t nb_op = 0;
  for (struct map_elem * e = backward ? m->last : m->first; e;)
  {
    struct map_elem *n = backward ? _map_previous (e) : _map_next (e);
    int remove = 0;
    int go_on = 1;
    if (!sel || sel (e->data, sel_arg))
    {
      if (op)
      {
        go_on = op (e->data, op_arg, &remove);
        n = backward ? _map_previous (e) : _map_next (e);       // Again after op is called. An added equal element while traversing might be traversed later.
      }
      nb_op++;
      if (remove)
        _map_remove (e);
      if (!go_on)
        break;
    }
    e = n;
  }
  mtx_unlock (&m->mutex);
  return nb_op;
}

size_t
map_traverse (map *m, map_operator op, void *op_arg, map_selector sel, void *sel_arg)
{
  return _map_traverse (m, op, op_arg, sel, sel_arg, 0);
}

size_t
map_traverse_backward (map *m, map_operator op, void *op_arg, map_selector sel, void *sel_arg)
{
  return _map_traverse (m, op, op_arg, sel, sel_arg, 1);
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
      struct map_elem *next = iter->eq_next;    // After op is called. An added equal element while finding will be found later.
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

const map_operator MAP_REMOVE_ONE = _MAP_REMOVE;

static int
_MAP_GET (void *data, void *context, int *remove)
{
  if (context)
    *(void **) context = data;  // *context is supposed to be a pointer here.
  *remove = 0;
  return 0;
}

const map_operator MAP_GET_ONE = _MAP_GET;

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

const map_operator MAP_REMOVE_ALL = _MAP_REMOVE_ALL;

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

const map_operator MAP_MOVE_TO = _MAP_MOVE;
