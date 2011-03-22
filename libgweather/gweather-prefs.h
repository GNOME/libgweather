/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather-prefs.h - Preference handling functions
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
 * along with this program; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef __GWEATHER_PREFS_H_
#define __GWEATHER_PREFS_H_


#ifndef GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#error "libgweather should only be used if you understand that it's subject to change, and is not supported as a fixed API/ABI or as part of the platform"
#endif


#include <libgweather/weather.h>
#include <libgweather/gweather-gconf.h>

/* gconf keys */
#define GCONF_TEMP_UNIT     "temperature_unit"
#define GCONF_SPEED_UNIT    "speed_unit"
#define GCONF_PRESSURE_UNIT "pressure_unit"
#define GCONF_DISTANCE_UNIT "distance_unit"

typedef struct _GWeatherPrefs GWeatherPrefs;

void	                gweather_prefs_load			(GWeatherPrefs *prefs,
								 GWeatherGConf *ctx);

const char *            gweather_prefs_temp_enum_to_string	(GWeatherTemperatureUnit temp);
const char *            gweather_prefs_speed_enum_to_string	(GWeatherSpeedUnit speed);
const char *            gweather_prefs_pressure_enum_to_string	(GWeatherPressureUnit pressure);
const char *            gweather_prefs_distance_enum_to_string	(GWeatherDistanceUnit distance);

GWeatherTemperatureUnit gweather_prefs_parse_temperature        (const char *str,
								 gboolean   *is_default);
GWeatherSpeedUnit       gweather_prefs_parse_speed              (const char *str,
								 gboolean   *is_default);

const char *            gweather_prefs_get_temp_display_name		(GWeatherTemperatureUnit temp);
const char *            gweather_prefs_get_speed_display_name		(GWeatherSpeedUnit speed);
const char *            gweather_prefs_get_pressure_display_name	(GWeatherPressureUnit pressure);
const char *            gweather_prefs_get_distance_display_name	(GWeatherDistanceUnit distance);

#endif /* __GWEATHER_PREFS_H_ */
