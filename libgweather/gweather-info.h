/* gweather-info.h - Weather information data
 *
 * SPDX-FileCopyrightText: The GWeather authors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !(defined(IN_GWEATHER_H) || defined(GWEATHER_COMPILATION))
#error "gweather-weather.h must not be included individually, include gweather.h instead"
#endif

#include <libgweather/gweather-enums.h>
#include <libgweather/gweather-location.h>
#include <libgweather/gweather-version.h>

G_BEGIN_DECLS

/**
 * GWeatherProvider:
 * @GWEATHER_PROVIDER_NONE: no provider, no weather information available
 * @GWEATHER_PROVIDER_METAR: METAR office, providing current conditions worldwide
 * @GWEATHER_PROVIDER_IWIN: US weather office (old API), providing 7 days of forecast
 * @GWEATHER_PROVIDER_MET_NO: MET.no service, worldwide but requires attribution and a subscription to the [API users mailing-list](https://lists.met.no/mailman/listinfo/api-users).
 * @GWEATHER_PROVIDER_OWM: OpenWeatherMap, worldwide and possibly more reliable, but requires attribution and is limited in the number of queries
 * @GWEATHER_PROVIDER_NWS: US weather office (new API), providing 7 days of hourly forecast (available since 4.2)
 * @GWEATHER_PROVIDER_ALL: enable all available providers
 */
typedef enum { /*< flags, underscore_name=gweather_provider >*/
    GWEATHER_PROVIDER_NONE = 0,
    GWEATHER_PROVIDER_METAR = 1,
    GWEATHER_PROVIDER_IWIN = 1 << 2,
    GWEATHER_PROVIDER_MET_NO = 1 << 3,
    GWEATHER_PROVIDER_OWM = 1 << 4,
    GWEATHER_PROVIDER_NWS = 1 << 5,
    GWEATHER_PROVIDER_ALL = (GWEATHER_PROVIDER_METAR | GWEATHER_PROVIDER_IWIN | GWEATHER_PROVIDER_MET_NO | GWEATHER_PROVIDER_OWM | GWEATHER_PROVIDER_NWS)
} GWeatherProvider;

#define GWEATHER_TYPE_INFO (gweather_info_get_type ())

GWEATHER_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GWeatherInfo, gweather_info, GWEATHER, INFO, GObject)

GWEATHER_AVAILABLE_IN_ALL
void                            gweather_info_store_cache               (void);

GWEATHER_AVAILABLE_IN_ALL
GWeatherInfo *                  gweather_info_new                       (GWeatherLocation *location);
GWEATHER_AVAILABLE_IN_ALL
void                            gweather_info_update                    (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
void                            gweather_info_abort                     (GWeatherInfo *info);

GWEATHER_AVAILABLE_IN_ALL
GWeatherProvider                gweather_info_get_enabled_providers     (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
void                            gweather_info_set_enabled_providers     (GWeatherInfo  *info,
                                                                         GWeatherProvider providers);

GWEATHER_AVAILABLE_IN_ALL
const char *                    gweather_info_get_application_id        (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
void                            gweather_info_set_application_id        (GWeatherInfo *info,
                                                                         const char *application_id);

GWEATHER_AVAILABLE_IN_ALL
const char *                    gweather_info_get_contact_info          (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
void                            gweather_info_set_contact_info          (GWeatherInfo *info,
                                                                         const char *contact_info);

GWEATHER_AVAILABLE_IN_ALL
gboolean                        gweather_info_is_valid                  (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
gboolean                        gweather_info_network_error             (GWeatherInfo *info);

GWEATHER_AVAILABLE_IN_ALL
const GWeatherLocation *        gweather_info_get_location              (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
void                            gweather_info_set_location              (GWeatherInfo *info,
                                                                         GWeatherLocation *location);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_location_name         (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_update                (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_sky                   (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_conditions            (GWeatherInfo *info);

GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_temp                  (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_temp_min              (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_temp_max              (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_dew                   (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_humidity              (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_wind                  (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_pressure              (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_visibility            (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_apparent              (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_sunrise               (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_sunset                (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
GSList *                        gweather_info_get_forecast_list         (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
const char *                    gweather_info_get_attribution           (GWeatherInfo *info);

GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_temp_summary          (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
char *                          gweather_info_get_weather_summary       (GWeatherInfo *info);

GWEATHER_AVAILABLE_IN_ALL
const char *                    gweather_info_get_icon_name             (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
const char *                    gweather_info_get_symbolic_icon_name    (GWeatherInfo *info);

GWEATHER_AVAILABLE_IN_ALL
int                             gweather_info_next_sun_event            (GWeatherInfo *info);
GWEATHER_AVAILABLE_IN_ALL
gboolean                        gweather_info_is_daytime                (GWeatherInfo *info);

/* values retrieving functions */

/**
 * GWeatherWindDirection:
 * @GWEATHER_WIND_INVALID: value not available
 * @GWEATHER_WIND_VARIABLE: variable throughout the day
 * @GWEATHER_WIND_N: north
 * @GWEATHER_WIND_NNE: north-north-east
 * @GWEATHER_WIND_NE: north-east
 * @GWEATHER_WIND_ENE: east-north-east
 * @GWEATHER_WIND_E: east
 * @GWEATHER_WIND_ESE: east-south-east
 * @GWEATHER_WIND_SE: south-east
 * @GWEATHER_WIND_SSE: south-south-east
 * @GWEATHER_WIND_S: south
 * @GWEATHER_WIND_SSW: south-south-west
 * @GWEATHER_WIND_SW: south-west
 * @GWEATHER_WIND_WSW: west-south-west
 * @GWEATHER_WIND_W: west
 * @GWEATHER_WIND_WNW: west-north-west
 * @GWEATHER_WIND_NW: north-west
 * @GWEATHER_WIND_NNW: north-north-west
 * @GWEATHER_WIND_LAST: maximum value for the enumeration
 *
 * The direction of the prevailing wind. Composite values
 * such as north-north-east indicate a direction between the
 * two component value (north and north-east).
 */
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

GWEATHER_AVAILABLE_IN_ALL
const char *    gweather_wind_direction_to_string       (GWeatherWindDirection wind);
GWEATHER_AVAILABLE_IN_ALL
const char *    gweather_wind_direction_to_string_full  (GWeatherWindDirection wind,
                                                         GWeatherFormatOptions options);

/**
 * GWeatherSky:
 * @GWEATHER_SKY_INVALID: value not available
 * @GWEATHER_SKY_CLEAR: sky completely clear, no clouds visible
 * @GWEATHER_SKY_BROKEN: sky mostly clear, few clouds
 * @GWEATHER_SKY_SCATTERED: sky mostly clear, patches of clouds
 * @GWEATHER_SKY_FEW: few clouds, sky cloudy but patches of sky visible
 * @GWEATHER_SKY_OVERCAST: sky completely clouded, sun not visible
 * @GWEATHER_SKY_LAST: the maximum value for the enumeration
 *
 * The sky and cloud visibility. In general it is discouraged to
 * use this value directly to compute the forecast icon: applications
 * should instead use gweather_info_get_icon_name() or
 * gweather_info_get_symbolic_icon_name().
 */
typedef enum { /*< underscore_name=gweather_sky >*/
    GWEATHER_SKY_INVALID = -1,
    GWEATHER_SKY_CLEAR,
    GWEATHER_SKY_BROKEN,
    GWEATHER_SKY_SCATTERED,
    GWEATHER_SKY_FEW,
    GWEATHER_SKY_OVERCAST,
    GWEATHER_SKY_LAST
} GWeatherSky;

GWEATHER_AVAILABLE_IN_ALL
const char *    gweather_sky_to_string          (GWeatherSky sky);
GWEATHER_AVAILABLE_IN_ALL
const char *    gweather_sky_to_string_full     (GWeatherSky sky,
                                                 GWeatherFormatOptions options);

/**
 * GWeatherConditionPhenomenon:
 * @GWEATHER_PHENOMENON_INVALID: value not available
 * @GWEATHER_PHENOMENON_NONE: no significant phenomenon
 *
 * The current or forecasted significant phenomenon.
 */
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

/**
 * GWeatherConditionQualifier:
 * @GWEATHER_QUALIFIER_INVALID: value not available
 * @GWEATHER_QUALIFIER_NONE: no qualifier for the phenomenon
 * @GWEATHER_QUALIFIER_VICINITY: phenomenon happening in the proximity of the
 *   location, not in the actual location
 * @GWEATHER_QUALIFIER_LIGHT: phenomenon is light or predicted to be light
 * @GWEATHER_QUALIFIER_MODERATE: phenomenon is moderate or predicted to be
 *   moderate
 * @GWEATHER_QUALIFIER_HEAVY: phenomenon is heavy or predicted to be heavy
 * @GWEATHER_QUALIFIER_SHALLOW: shallow fog (only valid with
 *   %GWEATHER_PHENOMENON_FOG)
 * @GWEATHER_QUALIFIER_PATCHES: patches of fog (only valid with
 *   %GWEATHER_PHENOMENON_FOG)
 * @GWEATHER_QUALIFIER_PARTIAL: partial fog (only valid with
 *   %GWEATHER_PHENOMENON_FOG)
 * @GWEATHER_QUALIFIER_THUNDERSTORM: phenomenon will be a thunderstorm and/or
 *   will include lightning
 * @GWEATHER_QUALIFIER_BLOWING: phenomenon is blowing (valid with
 *   %GWEATHER_PHENOMENON_SNOW, %GWEATHER_PHENOMENON_SAND,
 *   %GWEATHER_PHENOMENON_SPRAY, %GWEATHER_PHENOMENON_DUST)
 * @GWEATHER_QUALIFIER_SHOWERS: phenomenon is heavy and involves showers
 * @GWEATHER_QUALIFIER_DRIFTING: phenomenon is moving across (valid with
 *   %GWEATHER_PHENOMENON_SAND and %GWEATHER_PHENOMENON_DUST)
 * @GWEATHER_QUALIFIER_FREEZING: phenomenon is freezing and involves ice
 * @GWEATHER_QUALIFIER_LAST: maximum value of the enumeration
 *
 * An additional modifier applied to a #GWeatherConditionPhenomenon to
 * describe the current or forecasted weather conditions.
 *
 * The exact meaning of each qualifier is described at
 * http://www.weather.com/glossary/ and
 * http://www.crh.noaa.gov/arx/wx.tbl.php
 */
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

/**
 * GWeatherMoonPhase:
 *
 * The current phase of the moon, represented as degrees,
 * where 0 is the new moon, 90 is the first quarter, etc.
 */
typedef double GWeatherMoonPhase;

/**
 * GWeatherMoonLatitude:
 *
 * The moon declension, in degrees.
 */
typedef double GWeatherMoonLatitude;

GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_update          (GWeatherInfo *info,
                                                         time_t *value);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_sky             (GWeatherInfo *info,
                                                         GWeatherSky *sky);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_conditions      (GWeatherInfo *info,
                                                         GWeatherConditionPhenomenon *phenomenon,
                                                         GWeatherConditionQualifier *qualifier);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_temp            (GWeatherInfo *info,
                                                         GWeatherTemperatureUnit unit,
                                                         double *value);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_temp_min        (GWeatherInfo *info,
                                                         GWeatherTemperatureUnit unit,
                                                         double *value);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_temp_max        (GWeatherInfo *info,
                                                         GWeatherTemperatureUnit unit,
                                                         double *value);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_dew             (GWeatherInfo *info,
                                                         GWeatherTemperatureUnit unit,
                                                         double *value);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_apparent        (GWeatherInfo *info,
                                                         GWeatherTemperatureUnit unit,
                                                         double *value);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_wind            (GWeatherInfo *info,
                                                         GWeatherSpeedUnit unit,
                                                         double *speed,
                                                         GWeatherWindDirection *direction);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_pressure        (GWeatherInfo *info,
                                                         GWeatherPressureUnit unit,
                                                         double *value);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_visibility      (GWeatherInfo *info,
                                                         GWeatherDistanceUnit unit,
                                                         double *value);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_sunrise        (GWeatherInfo *info,
                                                         time_t *value);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_sunset          (GWeatherInfo *info,
                                                         time_t *value);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_value_moonphase       (GWeatherInfo *info,
                                                         GWeatherMoonPhase *value,
                                                         GWeatherMoonLatitude *lat);
GWEATHER_AVAILABLE_IN_ALL
gboolean        gweather_info_get_upcoming_moonphases   (GWeatherInfo *info,
                                                         time_t *phases);

typedef struct _GWeatherConditions GWeatherConditions;

/**
 * GWeatherConditions:
 * @significant: %TRUE if the struct contains usable data, %FALSE otherwise
 * @phenomenon: the main weather phenomenon
 * @qualifier: a modifier for @phenomenon
 *
 * A convenient way to describe the current or forecast
 * weather phenomenon, if significant, and its associated
 * modifier. If the value is not significant, the weather conditions
 * are described by gweather_info_get_sky() instead.
 *
 * In general it is discouraged to use this value directly to compute
 * the forecast icon: applications should instead use
 * gweather_info_get_icon_name() or
 * gweather_info_get_symbolic_icon_name().
 */
struct _GWeatherConditions {
    gboolean significant;
    GWeatherConditionPhenomenon phenomenon;
    GWeatherConditionQualifier qualifier;
};

GWEATHER_AVAILABLE_IN_ALL
const char *    gweather_conditions_to_string           (GWeatherConditions *conditions);
GWEATHER_AVAILABLE_IN_ALL
const char *    gweather_conditions_to_string_full      (GWeatherConditions *conditions,
                                                         GWeatherFormatOptions options);

GWEATHER_AVAILABLE_IN_ALL
GWeatherTemperatureUnit gweather_temperature_unit_to_real       (GWeatherTemperatureUnit unit);

GWEATHER_AVAILABLE_IN_ALL
const char *    gweather_speed_unit_to_string   (GWeatherSpeedUnit unit);

G_END_DECLS
