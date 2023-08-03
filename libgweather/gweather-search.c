/* gweather-search.c - Search-handling code
 *
 * SPDX-FileCopyrightText: 2023, Red Hat, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "gweather-private.h"
#include "gweather-search.h"

#include "search-index.h"

struct _GWeatherSearch
{
  GObject           parent_instance;
  SearchIndex      *index;
  GWeatherLocation *world;
};

G_DEFINE_TYPE (GWeatherSearch, gweather_search, G_TYPE_OBJECT)

static void
gweather_search_constructed (GObject *object)
{
  GWeatherSearch *self = GWEATHER_SEARCH (object);
  g_autofree char *filename = NULL;
  g_autofree char *directory = NULL;
  g_autoptr(GError) error = NULL;
  GWeatherDb *db;

  G_OBJECT_CLASS (gweather_search_parent_class)->constructed (object);

  if (!(db = gweather_get_db ()) || db->filename == NULL)
    {
      g_critical ("Failed to locate weather db");
      return;
    }

  if (!(self->world = gweather_location_get_world ()))
    {
      g_critical ("Failed to locate weather world");
      return;
    }

  directory = g_path_get_dirname (db->filename);
  filename = g_build_filename (directory, "Locations.index", NULL);

  if (!(self->index = search_index_new (filename, &error)))
    {
      g_critical ("Failed to locate weather search index: %s", error->message);
      return;
    }
}

static void
gweather_search_finalize (GObject *object)
{
  GWeatherSearch *self = GWEATHER_SEARCH (object);

  g_clear_pointer (&self->index, search_index_unref);
  g_clear_object (&self->world);

  G_OBJECT_CLASS (gweather_search_parent_class)->finalize (object);
}

static void
gweather_search_class_init (GWeatherSearchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gweather_search_constructed;
  object_class->finalize = gweather_search_finalize;
}

static void
gweather_search_init (GWeatherSearch *self)
{
}

static GWeatherSearch *
_gweather_search_new (void)
{
  return g_object_new (GWEATHER_TYPE_SEARCH, NULL);
}

/**
 * gweather_search_get_world:
 *
 * Returns: (transfer full): a #GWeatherSearch instance
 */
GWeatherSearch *
gweather_search_get_world (void)
{
  static GWeatherSearch *instance;

  if (g_once_init_enter (&instance))
    {
      GWeatherSearch *self = _gweather_search_new ();
      g_object_add_weak_pointer (G_OBJECT (self), (gpointer *)&instance);
      g_once_init_leave (&instance, self);
    }

  return g_object_ref (instance);
}

static GVariant *
deserialize_bytes (GBytes *bytes)
{
  GVariant *variant = g_variant_new_from_bytes (G_VARIANT_TYPE ("(uv)"), bytes, FALSE);
  g_bytes_unref (bytes);
  return variant;
}

static int
sort_by_size_asc (gconstpointer a,
                  gconstpointer b)
{
  const SearchIndexIter *iter_a = a;
  const SearchIndexIter *iter_b = b;
  gsize size_a = iter_a->end - iter_a->pos;
  gsize size_b = iter_b->end - iter_b->pos;

  if (size_a < size_b)
    return -1;
  else if (size_a > size_b)
    return 1;
  else
    return 0;
}

static GBytes *
next_match (SearchIndexIter *iters,
            guint            n_iters)
{
  SearchDocument document;
  g_autoptr(GBytes) bytes = NULL;

again:
  g_clear_pointer (&bytes, g_bytes_unref);

  if (!(bytes = search_index_iter_next_bytes (&iters[0], &document)))
    return FALSE;

  for (guint i = 1; i < n_iters; i++)
    {
      if (!search_index_iter_seek_to (&iters[i], document.id))
        goto again;
    }

  return g_steal_pointer (&bytes);
}

static gboolean
gweather_search_matches (GWeatherLocation   *location,
                         const char * const *terms,
                         guint               n_terms)
{
  g_autofree char *city_name = gweather_location_get_city_name (location);
  g_autofree char *city_name_normal = city_name ? g_utf8_normalize (city_name, -1, G_NORMALIZE_DEFAULT) : NULL;
  g_autofree char *city_name_casefold = city_name_normal ? g_utf8_casefold (city_name_normal, -1) : NULL;

  g_autofree char *country_name = gweather_location_get_country_name (location);
  g_autofree char *country_name_normal = country_name ? g_utf8_normalize (country_name, -1, G_NORMALIZE_DEFAULT) : NULL;
  g_autofree char *country_name_casefold = country_name_normal ? g_utf8_casefold (country_name_normal, -1) : NULL;

  for (guint i = 0; i < n_terms; i++)
    {
      if (city_name_casefold && strstr (city_name_casefold, terms[i]))
        continue;

      if (country_name_casefold && strstr (country_name_casefold, terms[i]))
        continue;

      return FALSE;
    }

  return TRUE;
}

/**
 * gweather_search_find_matching:
 * @self: a #GweatherSearch
 * @terms: (array zero-terminated=1) (element-type utf8): An array of UTF-8 encoded search terms
 *
 * Finds #GWeatherLocation that match @terms
 *
 * Returns: (transfer full): A #GListModel of #GWeatherLocation
 */
GListModel *
gweather_search_find_matching (GWeatherSearch     *self,
                               const char * const *terms)
{
  g_autoptr(GArray) iters = NULL;
  g_autoptr(GHashTable) seen = NULL;
  g_autoptr(GPtrArray) casefold_terms = NULL;
  g_autoptr(GListStore) matches = NULL;
  GBytes *bytes;

  g_return_val_if_fail (GWEATHER_IS_SEARCH (self), NULL);
  g_return_val_if_fail (terms != NULL, NULL);

  iters = g_array_new (FALSE, FALSE, sizeof (SearchIndexIter));
  seen = g_hash_table_new (NULL, NULL);
  casefold_terms = g_ptr_array_new_with_free_func (g_free);
  matches = g_list_store_new (GWEATHER_TYPE_LOCATION);

  for (guint i = 0; terms[i]; i++)
    {
      g_autofree char *query_normal = g_utf8_normalize (terms[i], -1, G_NORMALIZE_DEFAULT);
      g_autofree char *query_casefold = g_utf8_casefold (query_normal, -1);

      g_ptr_array_add (casefold_terms, g_steal_pointer (&query_casefold));
    }

  for (guint i = 0; i < casefold_terms->len; i++)
    {
      const char *query = g_ptr_array_index (casefold_terms, i);
      SearchTrigramIter iter;
      SearchTrigram trigram;

      search_trigram_iter_init (&iter, query, -1);
      while (search_trigram_iter_next (&iter, &trigram))
        {
          SearchIndexIter index_iter;
          guint encoded = search_trigram_encode (&trigram);

          if (g_hash_table_contains (seen, GUINT_TO_POINTER (encoded)))
            continue;

          search_index_iter_init (&index_iter, self->index, &trigram);
          g_array_append_val (iters, index_iter);
          g_hash_table_add (seen, GUINT_TO_POINTER (encoded));
        }
    }

  if (iters->len == 0)
    return G_LIST_MODEL (g_steal_pointer (&matches));

  g_array_sort (iters, sort_by_size_asc);

  while ((bytes = next_match (&g_array_index (iters, SearchIndexIter, 0), iters->len)))
    {
      g_autoptr(GVariant) variant = deserialize_bytes (g_steal_pointer (&bytes));

      if (variant != NULL)
        {
          g_autoptr(GWeatherLocation) location = gweather_location_deserialize (self->world, variant);

          /* At this point, it's pretty likely we're going to have a match because
           * we at least know that the location contains all of the characters that
           * we saw in our term list. They may not be in sequential order though, and
           * so that would be a negative match.
           */

          if (gweather_search_matches (location, (const char * const *)casefold_terms->pdata, casefold_terms->len))
            g_list_store_append (matches, location);
        }
    }

  return G_LIST_MODEL (g_steal_pointer (&matches));
}
