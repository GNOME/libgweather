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
#include <libgweather/gweather-enums.h>
#include <libgweather/gweather-location.h>

G_BEGIN_DECLS

/**
 * GWeatherProvider:
 * @GWEATHER_PROVIDER_NONE: no provider, no weather information available
 * @GWEATHER_PROVIDER_METAR: METAR office, providing current conditions worldwide
 * @GWEATHER_PROVIDER_IWIN: US weather office, providing 7 days of forecast
 * @GWEATHER_PROVIDER_YAHOO: Yahoo Weather Service, worldwide but non commercial only
 * @GWEATHER_PROVIDER_YR_NO: Yr.no service, worldwide but requires attribution
 * @GWEATHER_PROVIDER_ALL: enable all available providers
 */
typedef enum { /*< flags, underscore_name=gweather_provider >*/
    GWEATHER_PROVIDER_NONE = 0,
    GWEATHER_PROVIDER_METAR = 1,
    GWEATHER_PROVIDER_IWIN = 1 << 2,
    GWEATHER_PROVIDER_YAHOO = 1 << 3,
    GWEATHER_PROVIDER_YR_NO = 1 << 4,
    GWEATHER_PROVIDER_ALL = 31
} GWeatherProvider;

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

GType                    gweather_info_get_type            (void) G_GNUC_CONST;
GWeatherInfo *           gweather_info_new                 (GWeatherLocation *location,
							    GWeatherForecastType forecast_type);
GWeatherInfo *           gweather_info_new_for_world       (GWeatherLocation *world,
							    GWeatherLocation *location,
							    GWeatherForecastType forecast_type);
void                     gweather_info_update              (GWeatherInfo *info);
void			 gweather_info_abort		   (GWeatherInfo *info);

GWeatherProvider         gweather_info_get_enabled_providers (GWeatherInfo        *info);
void                     gweather_info_set_enabled_providers (GWeatherInfo        *info,
							      GWeatherProvider     providers);

gboolean		 gweather_info_is_valid		   (GWeatherInfo *info);
gboolean		 gweather_info_network_error	   (GWeatherInfo *info);

const GWeatherLocation * gweather_info_get_location	   (GWeatherInfo *info);
void                     gweather_info_set_location        (GWeatherInfo *info,
							    GWeatherLocation *location);
gchar *		         gweather_info_get_location_name   (GWeatherInfo *info);
gchar * 		 gweather_info_get_update	   (GWeatherInfo *info);
gchar * 		 gweather_info_get_sky		   (GWeatherInfo *info);
gchar *	                 gweather_info_get_conditions	   (GWeatherInfo *info);
gchar * 	  	 gweather_info_get_temp		   (GWeatherInfo *info);
gchar * 	 	 gweather_info_get_temp_min	   (GWeatherInfo *info);
gchar * 		 gweather_info_get_temp_max	   (GWeatherInfo *info);
gchar * 		 gweather_info_get_dew		   (GWeatherInfo *info);
gchar * 		 gweather_info_get_humidity	   (GWeatherInfo *info);
gchar * 		 gweather_info_get_wind		   (GWeatherInfo *info);
gchar * 		 gweather_info_get_pressure	   (GWeatherInfo *info);
gchar * 		 gweather_info_get_visibility	   (GWeatherInfo *info);
gchar * 		 gweather_info_get_apparent	   (GWeatherInfo *info);
gchar * 		 gweather_info_get_sunrise	   (GWeatherInfo *info);
gchar * 		 gweather_info_get_sunset	   (GWeatherInfo *info);
gchar * 		 gweather_info_get_forecast	   (GWeatherInfo *info);
GSList *		 gweather_info_get_forecast_list   (GWeatherInfo *info);
GdkPixbufAnimation *	 gweather_info_get_radar	   (GWeatherInfo *info);
const gchar             *gweather_info_get_attribution     (GWeatherInfo *info);

gchar * 		 gweather_info_get_temp_summary	   (GWeatherInfo *info);
gchar *			 gweather_info_get_weather_summary (GWeatherInfo *info);

const gchar *		 gweather_info_get_icon_name	   (GWeatherInfo *info);
gint			 gweather_info_next_sun_event	   (GWeatherInfo *info);

gboolean                 gweather_info_is_daytime          (GWeatherInfo *info);

/* values retrieving functions */

typedef enum { /*< underscore_name=gweather_wind_direction >*/
    GWEATHER_WIND_INVALID = -1,
    GWEATHER_WIND_VARIABLE,
    GWEATHER_WIND_N,
    GWEATHER_WIND_NNE,
    GWEATHER_WIND_NE,
    GWEATHER_WIND_ENE,
    GWEATHER_WIND_E,
    GWEATHER_WIND_ESE,
    GWEATHER_WIND_SE,
    GWEATHER_WIND_SSE,
    GWEATHER_WIND_S,
    GWEATHER_WIND_SSW,
    GWEATHER_WIND_SW,
    GWEATHER_WIND_WSW,
    GWEATHER_WIND_W,
    GWEATHER_WIND_WNW,
    GWEATHER_WIND_NW,
    GWEATHER_WIND_NNW,
    GWEATHER_WIND_LAST
} GWeatherWindDirection;

const gchar * gweather_wind_direction_to_string (GWeatherWindDirection wind);

typedef enum { /*< underscore_name=gweather_sky >*/
    GWEATHER_SKY_INVALID = -1,
    GWEATHER_SKY_CLEAR,
    GWEATHER_SKY_BROKEN,
    GWEATHER_SKY_SCATTERED,
    GWEATHER_SKY_FEW,
    GWEATHER_SKY_OVERCAST,
    GWEATHER_SKY_LAST
} GWeatherSky;

const gchar * gweather_sky_to_string (GWeatherSky sky);

typedef enum { /*< underscore_name=gweather_phenomenon >*/
    GWEATHER_PHENOMENON_INVALID = -1,

    GWEATHER_PHENOMENON_NONE,

    GWEATHER_PHENOMENON_DRIZZLE,
    GWEATHER_PHENOMENON_RAIN,
    GWEATHER_PHENOMENON_SNOW,
    GWEATHER_PHENOMENON_SNOW_GRAINS,
    GWEATHER_PHENOMENON_ICE_CRYSTALS,
    GWEATHER_PHENOMENON_ICE_PELLETS,
    GWEATHER_PHENOMENON_HAIL,
    GWEATHER_PHENOMENON_SMALL_HAIL,
    GWEATHER_PHENOMENON_UNKNOWN_PRECIPITATION,

    GWEATHER_PHENOMENON_MIST,
    GWEATHER_PHENOMENON_FOG,
    GWEATHER_PHENOMENON_SMOKE,
    GWEATHER_PHENOMENON_VOLCANIC_ASH,
    GWEATHER_PHENOMENON_SAND,
    GWEATHER_PHENOMENON_HAZE,
    GWEATHER_PHENOMENON_SPRAY,
    GWEATHER_PHENOMENON_DUST,

    GWEATHER_PHENOMENON_SQUALL,
    GWEATHER_PHENOMENON_SANDSTORM,
    GWEATHER_PHENOMENON_DUSTSTORM,
    GWEATHER_PHENOMENON_FUNNEL_CLOUD,
    GWEATHER_PHENOMENON_TORNADO,
    GWEATHER_PHENOMENON_DUST_WHIRLS,

    GWEATHER_PHENOMENON_LAST
} GWeatherConditionPhenomenon;

typedef enum { /*< underscore_name=gweather_qualifier >*/
    GWEATHER_QUALIFIER_INVALID = -1,

    GWEATHER_QUALIFIER_NONE,

    GWEATHER_QUALIFIER_VICINITY,

    GWEATHER_QUALIFIER_LIGHT,
    GWEATHER_QUALIFIER_MODERATE,
    GWEATHER_QUALIFIER_HEAVY,
    GWEATHER_QUALIFIER_SHALLOW,
    GWEATHER_QUALIFIER_PATCHES,
    GWEATHER_QUALIFIER_PARTIAL,
    GWEATHER_QUALIFIER_THUNDERSTORM,
    GWEATHER_QUALIFIER_BLOWING,
    GWEATHER_QUALIFIER_SHOWERS,
    GWEATHER_QUALIFIER_DRIFTING,
    GWEATHER_QUALIFIER_FREEZING,

    GWEATHER_QUALIFIER_LAST
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

typedef struct _GWeatherConditions GWeatherConditions;
struct _GWeatherConditions {
    gboolean significant;
    GWeatherConditionPhenomenon phenomenon;
    GWeatherConditionQualifier qualifier;
};

const gchar * gweather_conditions_to_string (GWeatherConditions *conditions);

G_END_DECLS

#endif /* __WEATHER_H_ */
