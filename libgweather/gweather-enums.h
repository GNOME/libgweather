/* gweather-enums.h: enumerations for GWeather settings
 *
 * SPDX-FileCopyrightText: 2011 Giovanni Campagna <scampa.giovanni@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !(defined(IN_GWEATHER_H) || defined(GWEATHER_COMPILATION))
#error "gweather-enums.h must not be included individually, include gweather.h instead"
#endif

#include <glib.h>

G_BEGIN_DECLS

/**
 * GWeatherTemperatureUnit:
 * @GWEATHER_TEMP_UNIT_INVALID: invalid unit
 * @GWEATHER_TEMP_UNIT_DEFAULT: the user preferred temperature unit
 * @GWEATHER_TEMP_UNIT_KELVIN: Kelvin (absolute) temperature scale
 * @GWEATHER_TEMP_UNIT_CENTIGRADE: Celsius temperature scale
 * @GWEATHER_TEMP_UNIT_FAHRENHEIT: Fahrenheit temperature scale
 *
 * The measure unit to use for temperature values, when retrieved by
 * the gweather_info_get_value_temp() family of functions.
 */
typedef enum { /*< underscore_name=gweather_temperature_unit >*/
    GWEATHER_TEMP_UNIT_INVALID = 0,
    GWEATHER_TEMP_UNIT_DEFAULT,
    GWEATHER_TEMP_UNIT_KELVIN,
    GWEATHER_TEMP_UNIT_CENTIGRADE,
    GWEATHER_TEMP_UNIT_FAHRENHEIT
} GWeatherTemperatureUnit;

/**
 * GWeatherSpeedUnit:
 * @GWEATHER_SPEED_UNIT_INVALID: invalid unit
 * @GWEATHER_SPEED_UNIT_DEFAULT: the user preferred speed unit
 * @GWEATHER_SPEED_UNIT_MS: meters per second
 * @GWEATHER_SPEED_UNIT_KPH: kilometers per hour
 * @GWEATHER_SPEED_UNIT_MPH: miles per hour
 * @GWEATHER_SPEED_UNIT_KNOTS: knots
 * @GWEATHER_SPEED_UNIT_BFT: Beaufort scale
 *
 * The measure unit to use for wind speed values, when retrieved by
 * gweather_info_get_value_wind().
 */
typedef enum { /*< underscore_name=gweather_speed_unit >*/
    GWEATHER_SPEED_UNIT_INVALID = 0,
    GWEATHER_SPEED_UNIT_DEFAULT,
    GWEATHER_SPEED_UNIT_MS,
    GWEATHER_SPEED_UNIT_KPH,
    GWEATHER_SPEED_UNIT_MPH,
    GWEATHER_SPEED_UNIT_KNOTS,
    GWEATHER_SPEED_UNIT_BFT
} GWeatherSpeedUnit;

/**
 * GWeatherPressureUnit:
 * @GWEATHER_PRESSURE_UNIT_INVALID: invalid unit
 * @GWEATHER_PRESSURE_UNIT_DEFAULT: the user preferred pressure unit
 * @GWEATHER_PRESSURE_UNIT_KPA: kiloPascal (* 10^3 Pa)
 * @GWEATHER_PRESSURE_UNIT_HPA: hectoPascal (* 10^2 Pa); also known
 *   as millibars, but formatted differently
 * @GWEATHER_PRESSURE_UNIT_MB: millibars; same as %GWEATHER_PRESSURE_UNIT_HPA
 * @GWEATHER_PRESSURE_UNIT_MM_HG: millimeters of mercury
 * @GWEATHER_PRESSURE_UNIT_INCH_HG: inches of mercury
 * @GWEATHER_PRESSURE_UNIT_ATM: atmospheres
 *
 * The measure unit to use for atmospheric pressure values, when
 * retrieved by gweather_info_get_value_pressure().
 */
typedef enum { /*< underscore_name=gweather_pressure_unit >*/
    GWEATHER_PRESSURE_UNIT_INVALID = 0,
    GWEATHER_PRESSURE_UNIT_DEFAULT,
    GWEATHER_PRESSURE_UNIT_KPA,
    GWEATHER_PRESSURE_UNIT_HPA,
    GWEATHER_PRESSURE_UNIT_MB,
    GWEATHER_PRESSURE_UNIT_MM_HG,
    GWEATHER_PRESSURE_UNIT_INCH_HG,
    GWEATHER_PRESSURE_UNIT_ATM
} GWeatherPressureUnit;

/**
 * GWeatherDistanceUnit:
 * @GWEATHER_DISTANCE_UNIT_INVALID: invalid unit
 * @GWEATHER_DISTANCE_UNIT_DEFAULT: the user preferred distance unit
 * @GWEATHER_DISTANCE_UNIT_METERS: meters
 * @GWEATHER_DISTANCE_UNIT_KM: kilometers (= 1000 meters)
 * @GWEATHER_DISTANCE_UNIT_MILES: miles
 *
 * The measure unit to use for sky visibility values, when retrieved
 * by gweather_info_get_value_visibility().
 */
typedef enum { /*< underscore_name=gweather_distance_unit >*/
    GWEATHER_DISTANCE_UNIT_INVALID = 0,
    GWEATHER_DISTANCE_UNIT_DEFAULT,
    GWEATHER_DISTANCE_UNIT_METERS,
    GWEATHER_DISTANCE_UNIT_KM,
    GWEATHER_DISTANCE_UNIT_MILES
} GWeatherDistanceUnit;

/**
 * GWeatherFormatOptions:
 * @GWEATHER_FORMAT_OPTION_DEFAULT: The default string format
 * @GWEATHER_FORMAT_OPTION_SENTENCE_CAPITALIZATION: Capitalize as if the string
 *   was starting a sentence
 * @GWEATHER_FORMAT_OPTION_NO_CAPITALIZATION: Capitalize as if the string was
 *   appearing within a sentence
 *
 * Format options to influence the text returned by the
 * `gweather_*_to_string_full()` functions.
 */
typedef enum { /*< underscore_name=gweather_format_options >*/
    GWEATHER_FORMAT_OPTION_DEFAULT                 = 0,
    GWEATHER_FORMAT_OPTION_SENTENCE_CAPITALIZATION = 1 << 0,
    GWEATHER_FORMAT_OPTION_NO_CAPITALIZATION       = 1 << 1
} GWeatherFormatOptions;

G_END_DECLS
