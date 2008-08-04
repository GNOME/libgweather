/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather-timezone.c - Timezones
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef __GWEATHER_TIMEZONE_H__
#define __GWEATHER_TIMEZONE_H__

#ifndef GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#error "libgweather should only be used if you understand that it's subject to change, and is not supported as a fixed API/ABI or as part of the platform"
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GWeatherTimezone GWeatherTimezone;

const char       *gweather_timezone_get_name       (GWeatherTimezone *zone);
const char       *gweather_timezone_get_tzid       (GWeatherTimezone *zone);
int               gweather_timezone_get_offset     (GWeatherTimezone *zone);
gboolean          gweather_timezone_has_dst        (GWeatherTimezone *zone);
int               gweather_timezone_get_dst_offset (GWeatherTimezone *zone);

GWeatherTimezone *gweather_timezone_ref            (GWeatherTimezone *zone);
void              gweather_timezone_unref          (GWeatherTimezone *zone);

G_END_DECLS

#endif /* __GWEATHER_TIMEZONE_H__ */
