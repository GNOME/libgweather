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

G_BEGIN_DECLS

typedef struct _GWeatherTimezone GWeatherTimezone;

GType gweather_timezone_get_type (void);
#define GWEATHER_TYPE_TIMEZONE (gweather_timezone_get_type ())

const char *            gweather_timezone_get_name              (GWeatherTimezone *zone);
const char *            gweather_timezone_get_tzid              (GWeatherTimezone *zone);
int                     gweather_timezone_get_offset            (GWeatherTimezone *zone);
gboolean                gweather_timezone_has_dst               (GWeatherTimezone *zone);
int                     gweather_timezone_get_dst_offset        (GWeatherTimezone *zone);

GWeatherTimezone *      gweather_timezone_ref                   (GWeatherTimezone *zone);
void                    gweather_timezone_unref                 (GWeatherTimezone *zone);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GWeatherTimezone, gweather_timezone_unref);

GWeatherTimezone *      gweather_timezone_get_utc               (void);
GWeatherTimezone *      gweather_timezone_get_by_tzid           (const char       *tzid);

G_END_DECLS
