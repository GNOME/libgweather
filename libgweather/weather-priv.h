/*
 *  Papadimitriou Spiros <spapadim+@cs.cmu.edu>
 *
 *  This code released under the GNU GPL.
 *  Read the file COPYING for more information.
 *
 *  Private header for weather server functions.
 *
 */

#ifndef __WEATHER_PRIV_H_
#define __WEATHER_PRIV_H_

#include <time.h>
#include <libgnomevfs/gnome-vfs.h>

#include "weather.h"

/*
 * Weather information.
 */

enum _WeatherWindDirection {
    WIND_VARIABLE,
    WIND_N, WIND_NNE, WIND_NE, WIND_ENE,
    WIND_E, WIND_ESE, WIND_SE, WIND_SSE,
    WIND_S, WIND_SSW, WIND_SW, WIND_WSW,
    WIND_W, WIND_WNW, WIND_NW, WIND_NNW
};

typedef enum _WeatherWindDirection WeatherWindDirection;

enum _WeatherSky {
    SKY_INVALID = -1,
    SKY_CLEAR,
    SKY_BROKEN,
    SKY_SCATTERED,
    SKY_FEW,
    SKY_OVERCAST
};

typedef enum _WeatherSky WeatherSky;

enum _WeatherConditionPhenomenon {
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
   PHENOMENON_DUST_WHIRLS
};

typedef enum _WeatherConditionPhenomenon WeatherConditionPhenomenon;

enum _WeatherConditionQualifier {
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
   QUALIFIER_FREEZING
};

typedef enum _WeatherConditionQualifier WeatherConditionQualifier;

struct _WeatherConditions {
    gboolean significant;
    WeatherConditionPhenomenon phenomenon;
    WeatherConditionQualifier qualifier;
};

typedef struct _WeatherConditions WeatherConditions;

typedef gdouble WeatherTemperature;
typedef gdouble WeatherHumidity;
typedef gint WeatherWindSpeed;
typedef gdouble WeatherPressure;
typedef gdouble WeatherVisibility;
typedef time_t WeatherUpdate;

struct _WeatherInfo {
    WeatherForecastType forecast_type;

    TempUnit temperature_unit;
    SpeedUnit speed_unit;
    PressureUnit pressure_unit;
    DistanceUnit distance_unit;

    gboolean valid;
    gboolean sunValid;
    WeatherLocation *location;
    WeatherUpdate update;
    WeatherSky sky;
    WeatherConditions cond;
    WeatherTemperature temp;
    WeatherTemperature dew;
    WeatherWindDirection wind;
    WeatherWindSpeed windspeed;
    WeatherPressure pressure;
    WeatherVisibility visibility;
    WeatherUpdate sunrise;
    WeatherUpdate sunset;
    gchar *forecast;
    gchar *metar_buffer;
    gchar *iwin_buffer;
    gchar *met_buffer;
    gchar *bom_buffer;
    gchar *radar_buffer;
    gchar *radar_url;
    GdkPixbufLoader *radar_loader;
    GdkPixbufAnimation *radar;
    GnomeVFSAsyncHandle *metar_handle;
    GnomeVFSAsyncHandle *iwin_handle;
    GnomeVFSAsyncHandle *wx_handle;
    GnomeVFSAsyncHandle *met_handle;
    GnomeVFSAsyncHandle *bom_handle;
    gboolean requests_pending;

    WeatherInfoFunc finish_cb;
    gpointer cb_data;
};

/*
 * Enum -> string conversions.
 */

const gchar *	weather_wind_direction_string	(WeatherWindDirection wind);
const gchar *	weather_sky_string		(WeatherSky sky);
const gchar *	weather_conditions_string	(WeatherConditions cond);

/* Values common to the parsing source files */

#define DATA_SIZE			5000

#define CONST_DIGITS			"0123456789"
#define CONST_ALPHABET			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"

/* Units conversions and names */

#define TEMP_F_TO_C(f)			(((f) - 32.0) * 0.555556)
#define TEMP_F_TO_K(f)			(TEMP_F_TO_C(f) + 273.15)
#define TEMP_C_TO_F(c)			(((c) * 1.8) + 32.0)

#define WINDSPEED_KNOTS_TO_KPH(knots)	((knots) * 1.851965)
#define WINDSPEED_KNOTS_TO_MPH(knots)	((knots) * 1.150779)
#define WINDSPEED_KNOTS_TO_MS(knots)	((knots) * 0.514444)
/* 1 bft ~= (1 m/s / 0.836) ^ (2/3) */
#define WINDSPEED_KNOTS_TO_BFT(knots)	(pow ((knots) * 0.615363, 0.666666))

#define PRESSURE_INCH_TO_KPA(inch)	((inch) * 3.386)
#define PRESSURE_INCH_TO_HPA(inch)	((inch) * 33.86)
#define PRESSURE_INCH_TO_MM(inch)	((inch) * 25.40005)
#define PRESSURE_INCH_TO_MB(inch)	(PRESSURE_INCH_TO_HPA(inch))
#define PRESSURE_INCH_TO_ATM(inch)	((inch) * 0.033421052)
#define PRESSURE_MBAR_TO_INCH(mbar)	((mbar) * 0.029533373)

#define VISIBILITY_SM_TO_KM(sm)		((sm) * 1.609344)
#define VISIBILITY_SM_TO_M(sm)		(VISIBILITY_SM_TO_KM(sm) * 1000)

#define DEGREES_TO_RADIANS(deg)		((fmod(deg,360.) / 180.) * M_PI)
#define RADIANS_TO_DEGREES(rad)		((rad) * 180. / M_PI)
#define RADIANS_TO_HOURS(rad)		((rad) * 12. / M_PI)

void		metar_start_open	(WeatherInfo *info);
void		iwin_start_open		(WeatherInfo *info);
void		metoffice_start_open	(WeatherInfo *info);
void		bom_start_open		(WeatherInfo *info);
void		wx_start_open		(WeatherInfo *info);

gboolean	metar_parse		(gchar *metar,
					 WeatherInfo *info);

gboolean	requests_init		(WeatherInfo *info);
void		request_done		(GnomeVFSAsyncHandle *handle,
					 WeatherInfo *info);
void		requests_done_check	(WeatherInfo *info);

gboolean	calc_sun		(WeatherInfo *info);

#endif /* __WEATHER_PRIV_H_ */

