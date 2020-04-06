/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather-timezone.c - Timezone handling
 *
 * Copyright 2008, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <https://www.gnu.org/licenses/>.
 */

#ifndef __GWEATHER_TIMEZONE_H__
#define __GWEATHER_TIMEZONE_H__

#if !(defined(IN_GWEATHER_H) || defined(GWEATHER_COMPILATION))
#error "gweather-timezone.h must not be included individually, include gweather.h instead"
#endif

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GWeatherTimezone GWeatherTimezone;

GWEATHER_EXTERN
GType gweather_timezone_get_type (void);
#define GWEATHER_TYPE_TIMEZONE (gweather_timezone_get_type ())

GWEATHER_EXTERN
const char       *gweather_timezone_get_name       (GWeatherTimezone *zone);
GWEATHER_EXTERN
const char       *gweather_timezone_get_tzid       (GWeatherTimezone *zone);
GWEATHER_EXTERN
int               gweather_timezone_get_offset     (GWeatherTimezone *zone);
GWEATHER_EXTERN
gboolean          gweather_timezone_has_dst        (GWeatherTimezone *zone);
GWEATHER_EXTERN
int               gweather_timezone_get_dst_offset (GWeatherTimezone *zone);

GWEATHER_EXTERN
GWeatherTimezone *gweather_timezone_ref            (GWeatherTimezone *zone);
GWEATHER_EXTERN
void              gweather_timezone_unref          (GWeatherTimezone *zone);

GWEATHER_EXTERN
GWeatherTimezone *gweather_timezone_get_utc        (void);
GWEATHER_EXTERN
GWeatherTimezone *gweather_timezone_get_by_tzid    (const char *tzid);

G_END_DECLS

#endif /* __GWEATHER_TIMEZONE_H__ */
