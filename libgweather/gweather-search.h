/* gweather-search.h - Search-handling code
 *
 * SPDX-FileCopyrightText: 2023, Red Hat, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !(defined(IN_GWEATHER_H) || defined(GWEATHER_COMPILATION))
#error "gweather-search.h must not be included individually, include gweather.h instead"
#endif

#include <glib-object.h>
#include <libgweather/gweather-version.h>

G_BEGIN_DECLS

#define GWEATHER_TYPE_SEARCH (gweather_search_get_type ())

/**
 * GWeatherSearch:
 *
 * A `GWeatherSearch` represents a search index of locations that can
 * be queried with relatively low overhead.
 */
GWEATHER_AVAILABLE_IN_4_4
G_DECLARE_FINAL_TYPE (GWeatherSearch, gweather_search, GWEATHER, SEARCH, GObject)

GWEATHER_AVAILABLE_IN_4_4
GWeatherSearch *gweather_search_get_world     (void);
GWEATHER_AVAILABLE_IN_4_4
GListModel     *gweather_search_find_matching (GWeatherSearch     *self,
                                               const char * const *terms);

G_END_DECLS
