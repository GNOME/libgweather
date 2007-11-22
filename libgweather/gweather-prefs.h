/*
 *  Papadimitriou Spiros <spapadim+@cs.cmu.edu>
 *
 *  This code released under the GNU GPL.
 *  Read the file COPYING for more information.
 *
 *  Preference handling functions.
 *
 */

#ifndef __GWEATHER_PREFS_H_
#define __GWEATHER_PREFS_H_

#include <libgweather/weather.h>
#include <libgweather/gweather-gconf.h>

/* gconf keys */
#define GCONF_TEMP_UNIT     "temperature_unit"
#define GCONF_SPEED_UNIT    "speed_unit"
#define GCONF_PRESSURE_UNIT "pressure_unit"
#define GCONF_DISTANCE_UNIT "distance_unit"

typedef struct _GWeatherPrefs GWeatherPrefs;

struct _GWeatherPrefs {
    WeatherLocation *location;
    gint update_interval;  /* in seconds */
    gboolean update_enabled;
    gboolean detailed;
    gboolean radar_enabled;
    gboolean use_custom_radar_url;
    gchar *radar;
	
    TempUnit     temperature_unit;
    gboolean     use_temperature_default;
    SpeedUnit    speed_unit;
    gboolean     use_speed_default;
    PressureUnit pressure_unit;
    gboolean     use_pressure_default;
    DistanceUnit distance_unit;
    gboolean     use_distance_default;
};

void		gweather_prefs_load			(GWeatherPrefs *prefs,
							 GWeatherGConf *ctx);

const char *	gweather_prefs_temp_enum_to_string	(TempUnit temp);
const char *	gweather_prefs_speed_enum_to_string	(SpeedUnit speed);
const char *	gweather_prefs_pressure_enum_to_string	(PressureUnit pressure);
const char *	gweather_prefs_distance_enum_to_string	(DistanceUnit distance);


#endif /* __GWEATHER_PREFS_H_ */
