/* gweather-timezone.c - Timezone handling
 *
 * SPDX-FileCopyrightText: 2008, Red Hat, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !(defined(IN_GWEATHER_H) || defined(GWEATHER_COMPILATION))
#error "gweather-timezone.h must not be included individually, include gweather.h instead"
#endif

#include <glib-object.h>
#include <libgweather/gweather-version.h>

G_BEGIN_DECLS

typedef struct _GWeatherTimezone GWeatherTimezone;

GWEATHER_AVAILABLE_IN_ALL
GType gweather_timezone_get_type (void);
#define GWEATHER_TYPE_TIMEZONE (gweather_timezone_get_type ())

GWEATHER_AVAILABLE_IN_ALL
const char *            gweather_timezone_get_name              (GWeatherTimezone *zone);
GWEATHER_AVAILABLE_IN_ALL
const char *            gweather_timezone_get_tzid              (GWeatherTimezone *zone);
GWEATHER_AVAILABLE_IN_ALL
int                     gweather_timezone_get_offset            (GWeatherTimezone *zone);
GWEATHER_AVAILABLE_IN_ALL
gboolean                gweather_timezone_has_dst               (GWeatherTimezone *zone);
GWEATHER_AVAILABLE_IN_ALL
int                     gweather_timezone_get_dst_offset        (GWeatherTimezone *zone);

GWEATHER_AVAILABLE_IN_ALL
GWeatherTimezone *      gweather_timezone_ref                   (GWeatherTimezone *zone);
GWEATHER_AVAILABLE_IN_ALL
void                    gweather_timezone_unref                 (GWeatherTimezone *zone);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GWeatherTimezone, gweather_timezone_unref);

GWEATHER_AVAILABLE_IN_ALL
GWeatherTimezone *      gweather_timezone_get_utc               (void);
GWEATHER_AVAILABLE_IN_ALL
GWeatherTimezone *      gweather_timezone_get_by_tzid           (const char       *tzid);

G_END_DECLS
