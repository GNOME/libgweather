/* gweather-search-indexer.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <libgweather/gweather.h>

#include "search-index.h"

static inline void
add_trigrams (SearchIndexBuilder *builder,
              const char         *str)
{
  SearchTrigramIter iter;
  SearchTrigram trigram;

  search_trigram_iter_init (&iter, str, -1);
  while (search_trigram_iter_next (&iter, &trigram))
    search_index_builder_add (builder, &trigram);
}

static void
gweather_index_location_recurse (SearchIndexBuilder *builder,
                                 GWeatherLocation   *location)
{
  g_autoptr(GWeatherLocation) child = NULL;
  GWeatherLocationLevel level = gweather_location_get_level (location);

  if (level >= GWEATHER_LOCATION_CITY)
    {
      g_autoptr(GVariant) variant = gweather_location_serialize (location);
      g_autoptr(GWeatherLocation) first_child = gweather_location_next_child (location, NULL);
      g_autoptr(GBytes) bytes = NULL;

      /* Ignore cities with no children */
      if (level == GWEATHER_LOCATION_CITY && first_child == NULL)
        return;

      if (variant != NULL &&
          (bytes = g_variant_get_data_as_bytes (variant)))
        {
          g_autofree char *city_name = gweather_location_get_city_name (location);
          g_autofree char *country_name = gweather_location_get_country_name (location);

          if (city_name != NULL || country_name != NULL)
            {
              search_index_builder_begin (builder, bytes);

              if (city_name != NULL)
                {
                  g_autofree char *city_name_normal = g_utf8_normalize (city_name, -1, G_NORMALIZE_DEFAULT);
                  g_autofree char *city_name_casefold = g_utf8_casefold (city_name_normal, -1);

                  add_trigrams (builder, city_name_casefold);
                }

              if (country_name != NULL)
                {
                  g_autofree char *country_name_normal = g_utf8_normalize (country_name, -1, G_NORMALIZE_DEFAULT);
                  g_autofree char *country_name_casefold = g_utf8_casefold (country_name_normal, -1);

                  add_trigrams (builder, country_name_casefold);
                }

              search_index_builder_commit (builder);
            }
        }
    }

  while ((child = gweather_location_next_child (location, child)))
    gweather_index_location_recurse (builder, child);
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(SearchIndexBuilder) builder = NULL;
  g_autoptr(GWeatherLocation) world = NULL;
  g_autoptr(GError) error = NULL;
  const char *filename;

  if (argc != 2)
    {
      g_printerr ("usage: %s OUTFILE\n", argv[0]);
      return 1;
    }

  filename = argv[1];
  builder = search_index_builder_new ();
  world = gweather_location_get_world ();

  gweather_index_location_recurse (builder, world);

  if (!search_index_builder_write (builder, filename, &error))
    g_error ("%s", error->message);

  return 0;
}
