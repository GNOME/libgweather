/* search-sparse-set.h
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

G_GNUC_CONST
static inline gboolean
_ispow2 (guint v)
{
  return (v != 0) && ((v & (v - 1)) == 0);
}

typedef struct _SearchSparseSetItem
{
  guint value;
  guint user_value;
} SearchSparseSetItem;

typedef struct _SearchSparseSet
{
  SearchSparseSetItem *dense;
  guint             *sparse;
  guint              capacity;
  guint              len;
} SearchSparseSet;

#define CODE_SPARSE_SET_INIT(max) \
  (SearchSparseSet) { \
    .dense = g_new (SearchSparseSetItem, 8), \
    .sparse = g_new (guint, max), \
    .capacity = max, \
    .len = 0, \
  }

static inline void
search_sparse_set_init (SearchSparseSet *sparse_set,
                        guint            max)
{
  *sparse_set = CODE_SPARSE_SET_INIT (max);
}

static inline void
search_sparse_set_clear (SearchSparseSet *sparse_set)
{
  g_clear_pointer (&sparse_set->dense, g_free);
  g_clear_pointer (&sparse_set->sparse, g_free);
  sparse_set->len = 0;
  sparse_set->capacity = 0;
}

static inline void
search_sparse_set_reset (SearchSparseSet *sparse_set)
{
  if (sparse_set->len != 0)
    {
      sparse_set->dense = g_renew (SearchSparseSetItem, sparse_set->dense, 8);
      sparse_set->len = 0;
    }
}

static inline gboolean
search_sparse_set_add_with_data (SearchSparseSet *sparse_set,
                                 guint            value,
                                 guint            user_value)
{
  guint idx;

  g_return_val_if_fail (value < sparse_set->capacity, FALSE);

  idx = sparse_set->sparse[value];

  if (idx < sparse_set->len && sparse_set->dense[idx].value == value)
    return FALSE;

  idx = sparse_set->len;
  sparse_set->dense[idx].value = value;
  sparse_set->dense[idx].user_value = user_value;
  sparse_set->sparse[value] = idx;

  sparse_set->len++;

  if (sparse_set->len < 8)
    return TRUE;

  if (_ispow2 (sparse_set->len))
    sparse_set->dense = g_renew (SearchSparseSetItem, sparse_set->dense, sparse_set->len * 2);

  return TRUE;
}

static inline gboolean
search_sparse_set_add (SearchSparseSet *sparse_set,
                       guint            value)
{
  return search_sparse_set_add_with_data (sparse_set, value, 0);
}

static inline gboolean
search_sparse_set_contains (SearchSparseSet *sparse_set,
                            guint            value)
{
  guint idx;

  if (value >= sparse_set->capacity)
    return FALSE;

  idx = sparse_set->sparse[value];

  return idx < sparse_set->len && sparse_set->dense[idx].value == value;
}

static inline gboolean
search_sparse_set_get (SearchSparseSet *sparse_set,
                       guint            value,
                       guint           *user_value)
{
  guint idx;

  if (value >= sparse_set->capacity)
    return FALSE;

  idx = sparse_set->sparse[value];

  if (idx < sparse_set->len && sparse_set->dense[idx].value == value)
    {
      *user_value = sparse_set->dense[idx].user_value;
      return TRUE;
    }

  return FALSE;
}

static inline int
_search_sparse_set_compare (gconstpointer          a,
                            gconstpointer          b,
                            G_GNUC_UNUSED gpointer data)
{
  const SearchSparseSetItem *aval = a;
  const SearchSparseSetItem *bval = b;

  if (aval->value < bval->value)
    return -1;
  else if (aval->value > bval->value)
    return 1;
  else
    return 0;
}

static inline void
search_sparse_set_sort (SearchSparseSet *sparse_set)
{
  if (sparse_set->len < 2)
    return;

  g_qsort_with_data (sparse_set->dense,
                     sparse_set->len,
                     sizeof sparse_set->dense[0],
                     _search_sparse_set_compare,
                     NULL);

  for (guint i = 0; i < sparse_set->len; i++)
    sparse_set->sparse[sparse_set->dense[i].value] = i;
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (SearchSparseSet, search_sparse_set_clear)

G_END_DECLS
