/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* weather.h - Public header for weather server functions.
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

#ifndef __WEATHER_H_
#define __WEATHER_H_


#ifndef GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#error "libgweather should only be used if you understand that it's subject to change, and is not supported as a fixed API/ABI or as part of the platform"
#endif


#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgweather/gweather-location.h>

G_BEGIN_DECLS

/*
 * Weather prefs
 */

typedef enum _GWeatherForecastType {
    FORECAST_STATE,
    FORECAST_ZONE,
    FORECAST_LIST
} GWeatherForecastType;

typedef enum _GWeatherTempeatureUnit {
    TEMP_UNIT_INVALID = 0,
    TEMP_UNIT_DEFAULT,
    TEMP_UNIT_KELVIN,
    TEMP_UNIT_CENTIGRADE,
    TEMP_UNIT_FAHRENHEIT
} GWeatherTemperatureUnit;

typedef enum _GWeatherSpeedUnit {
    SPEED_UNIT_INVALID = 0,
    SPEED_UNIT_DEFAULT,
    SPEED_UNIT_MS,    /* metres per second */
    SPEED_UNIT_KPH,   /* kilometres per hour */
    SPEED_UNIT_MPH,   /* miles per hour */
    SPEED_UNIT_KNOTS, /* Knots */
    SPEED_UNIT_BFT    /* Beaufort scale */
} GWeatherSpeedUnit;

typedef enum _GWeatherPressureUnit {
    PRESSURE_UNIT_INVALID = 0,
    PRESSURE_UNIT_DEFAULT,
    PRESSURE_UNIT_KPA,    /* kiloPascal */
    PRESSURE_UNIT_HPA,    /* hectoPascal */
    PRESSURE_UNIT_MB,     /* 1 millibars = 1 hectoPascal */
    PRESSURE_UNIT_MM_HG,  /* millimeters of mecury */
    PRESSURE_UNIT_INCH_HG, /* inches of mercury */
    PRESSURE_UNIT_ATM     /* atmosphere */
} GWeatherPressureUnit;

typedef enum _GWeatherDistanceUnit {
    DISTANCE_UNIT_INVALID = 0,
    DISTANCE_UNIT_DEFAULT,
    DISTANCE_UNIT_METERS,
    DISTANCE_UNIT_KM,
    DISTANCE_UNIT_MILES
} GWeatherDistanceUnit;

#if 0
struct _GWeatherPrefs {
    GWeatherForecastType type;

    gboolean radar;
    const char *radar_custom_url;

    GWeatherTemperatureUnit temperature_unit;
    GWeatherSpeedUnit speed_unit;
    GWeatherPressureUnit pressure_unit;
    GWeatherDistanceUnit distance_unit;
};
#endif

typedef struct _GWeatherPrefs GWeatherPrefs;

/*
 * Weather Info
 */

typedef struct _GWeatherInfo GWeatherInfo;
typedef struct _GWeatherInfoClass GWeatherInfoClass;
typedef struct _GWeatherInfoPrivate GWeatherInfoPrivate;

#define GWEATHER_TYPE_INFO                  (gweather_info_get_type ())
#define GWEATHER_INFO(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GWEATHER_TYPE_INFO, GWeatherInfo))
#define GWEATHER_IS_INFO(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GWEATHER_TYPE_INFO))
#define GWEATHER_INFO_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GWEATHER_TYPE_INFO, GWeatherInfoClass))
#define GWEATHER_IS_INFO_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GWEATHER_TYPE_INFO))
#define GWEATHER_INFO_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GWEATHER_TYPE_INFO, GWeatherInfoClass))

struct _GWeatherInfo {
    /*< private >*/
    GObject parent_instance;

    GWeatherInfoPrivate *priv;
};

struct _GWeatherInfoClass {
    /*< private >*/
    GObjectClass parent_class;

    /*< protected >*/
    void (*updated) (GWeatherInfo *info);
};

typedef void (*GWeatherInfoFunc) (GWeatherInfo *info, gpointer data);

GType                    gweather_info_get_type            (void) G_GNUC_CONST;
GWeatherInfo *           gweather_info_new                 (GWeatherLocation *location,
							    GWeatherForecastType forecast_type,
							    const GWeatherPrefs *prefs);
void                     gweather_info_set_preferences     (GWeatherInfo *info,
							    const GWeatherPrefs *prefs);
void                     gweather_info_update              (GWeatherInfo *info);
void			 gweather_info_abort		   (GWeatherInfo *info);

gboolean		 gweather_info_is_valid		   (GWeatherInfo *info);
gboolean		 gweather_info_network_error	   (GWeatherInfo *info);

void			 gweather_info_to_metric	   (GWeatherInfo *info);
void			 gweather_info_to_imperial	   (GWeatherInfo *info);

const GWeatherLocation * gweather_info_get_location	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_location_name   (GWeatherInfo *info);
const gchar *		 gweather_info_get_update	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_sky		   (GWeatherInfo *info);
const gchar *		 gweather_info_get_conditions	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_temp		   (GWeatherInfo *info);
const gchar *		 gweather_info_get_temp_min	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_temp_max	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_dew		   (GWeatherInfo *info);
const gchar *		 gweather_info_get_humidity	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_wind		   (GWeatherInfo *info);
const gchar *		 gweather_info_get_pressure	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_visibility	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_apparent	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_sunrise	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_sunset	   (GWeatherInfo *info);
const gchar *		 gweather_info_get_forecast	   (GWeatherInfo *info);
GSList *		 gweather_info_get_forecast_list   (GWeatherInfo *info);
GdkPixbufAnimation *	 gweather_info_get_radar	   (GWeatherInfo *info);

const gchar *		 gweather_info_get_temp_summary	   (GWeatherInfo *info);
gchar *			 gweather_info_get_weather_summary (GWeatherInfo *info);

const gchar *		 gweather_info_get_icon_name	   (GWeatherInfo *info);
gint			 gweather_info_next_sun_event	   (GWeatherInfo *info);

/* values retrieving functions */

typedef enum _GWeatherWindDirection {
    WIND_INVALID = -1,
    WIND_VARIABLE,
    WIND_N, WIND_NNE, WIND_NE, WIND_ENE,
    WIND_E, WIND_ESE, WIND_SE, WIND_SSE,
    WIND_S, WIND_SSW, WIND_SW, WIND_WSW,
    WIND_W, WIND_WNW, WIND_NW, WIND_NNW,
    WIND_LAST
} GWeatherWindDirection;

const gchar * gweather_wind_direction_to_string (GWeatherWindDirection wind);

typedef enum _GWeatherSky {
    SKY_INVALID = -1,
    SKY_CLEAR,
    SKY_BROKEN,
    SKY_SCATTERED,
    SKY_FEW,
    SKY_OVERCAST,
    SKY_LAST
} GWeatherSky;

const gchar * gweather_sky_to_string (GWeatherSky sky);

typedef enum _GWeatherConditionPhenomenon {
    PHENOMENON_INVALID = -1,

    PHENOMENON_NONE,

    PHENOMENON_DRIZZLE,
    PHENOMENON_RAIN,
    PHENOMENON_SNOW,
    PHENOMENON_SNOW_GRAINS,
    PHENOMENON_ICE_CRYSTALS,
    PHENOMENON_ICE_PELLETS,
    PHENOMENON_HAIL,
    PHENOMENON_SMALL_HAIL,
    PHENOMENON_UNKNOWN_PRECIPITATION,

    PHENOMENON_MIST,
    PHENOMENON_FOG,
    PHENOMENON_SMOKE,
    PHENOMENON_VOLCANIC_ASH,
    PHENOMENON_SAND,
    PHENOMENON_HAZE,
    PHENOMENON_SPRAY,
    PHENOMENON_DUST,

    PHENOMENON_SQUALL,
    PHENOMENON_SANDSTORM,
    PHENOMENON_DUSTSTORM,
    PHENOMENON_FUNNEL_CLOUD,
    PHENOMENON_TORNADO,
    PHENOMENON_DUST_WHIRLS,

    PHENOMENON_LAST
} GWeatherConditionPhenomenon;

typedef enum _GWeatherConditionQualifier {
    QUALIFIER_INVALID = -1,

    QUALIFIER_NONE,

    QUALIFIER_VICINITY,

    QUALIFIER_LIGHT,
    QUALIFIER_MODERATE,
    QUALIFIER_HEAVY,
    QUALIFIER_SHALLOW,
    QUALIFIER_PATCHES,
    QUALIFIER_PARTIAL,
    QUALIFIER_THUNDERSTORM,
    QUALIFIER_BLOWING,
    QUALIFIER_SHOWERS,
    QUALIFIER_DRIFTING,
    QUALIFIER_FREEZING,

    QUALIFIER_LAST
} GWeatherConditionQualifier;

typedef gdouble GWeatherMoonPhase;
typedef gdouble GWeatherMoonLatitude;

gboolean gweather_info_get_value_update		(GWeatherInfo *info, time_t *value);
gboolean gweather_info_get_value_sky		(GWeatherInfo *info, GWeatherSky *sky);
gboolean gweather_info_get_value_conditions	(GWeatherInfo *info, GWeatherConditionPhenomenon *phenomenon, GWeatherConditionQualifier *qualifier);
gboolean gweather_info_get_value_temp		(GWeatherInfo *info, GWeatherTemperatureUnit unit, gdouble *value);
gboolean gweather_info_get_value_temp_min	(GWeatherInfo *info, GWeatherTemperatureUnit unit, gdouble *value);
gboolean gweather_info_get_value_temp_max	(GWeatherInfo *info, GWeatherTemperatureUnit unit, gdouble *value);
gboolean gweather_info_get_value_dew		(GWeatherInfo *info, GWeatherTemperatureUnit unit, gdouble *value);
gboolean gweather_info_get_value_apparent	(GWeatherInfo *info, GWeatherTemperatureUnit unit, gdouble *value);
gboolean gweather_info_get_value_wind		(GWeatherInfo *info, GWeatherSpeedUnit unit, gdouble *speed, GWeatherWindDirection *direction);
gboolean gweather_info_get_value_pressure	(GWeatherInfo *info, GWeatherPressureUnit unit, gdouble *value);
gboolean gweather_info_get_value_visibility	(GWeatherInfo *info, GWeatherDistanceUnit unit, gdouble *value);
gboolean gweather_info_get_value_sunrise	(GWeatherInfo *info, time_t *value);
gboolean gweather_info_get_value_sunset 	(GWeatherInfo *info, time_t *value);
gboolean gweather_info_get_value_moonphase      (GWeatherInfo *info, GWeatherMoonPhase *value, GWeatherMoonLatitude *lat);
gboolean gweather_info_get_upcoming_moonphases  (GWeatherInfo *info, time_t *phases);

typedef struct _GWeatherConditions {
    gboolean significant;
    GWeatherConditionPhenomenon phenomenon;
    GWeatherConditionQualifier qualifier;
} GWeatherConditions;

const gchar * gweather_conditions_to_string (GWeatherConditions *conditions);

G_END_DECLS

#endif /* __WEATHER_H_ */
