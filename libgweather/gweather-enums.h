/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* gweather-enums.h: enumerations for GWeather settings
 *
 * Copyright: 2011 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#ifndef __GWEATHER_ENUMS_H_
#define __GWEATHER_ENUMS_H_

#if !(defined(IN_GWEATHER_H) || defined(GWEATHER_COMPILATION))
#error "gweather-enums.h must not be included individually, include gweather.h instead"
#endif

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
    GWEATHER_SPEED_UNIT_MS,    /* metres per second */
    GWEATHER_SPEED_UNIT_KPH,   /* kilometres per hour */
    GWEATHER_SPEED_UNIT_MPH,   /* miles per hour */
    GWEATHER_SPEED_UNIT_KNOTS, /* Knots */
    GWEATHER_SPEED_UNIT_BFT    /* Beaufort scale */
} GWeatherSpeedUnit;

/**
 * GWeatherPressureUnit:
 * @GWEATHER_PRESSURE_UNIT_INVALID: invalid unit
 * @GWEATHER_PRESSURE_UNIT_DEFAULT: the user preferred pressure unit
 * @GWEATHER_PRESSURE_UNIT_KPA: kiloPascal (* 10^3 Pa)
 * @GWEATHER_PRESSURE_UNIT_HPA: hectoPascal (* 10^2 Pa); also known
 *                              as millibars, but formatted differently
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
    GWEATHER_PRESSURE_UNIT_KPA,    /* kiloPascal */
    GWEATHER_PRESSURE_UNIT_HPA,    /* hectoPascal */
    GWEATHER_PRESSURE_UNIT_MB,     /* 1 millibars = 1 hectoPascal */
    GWEATHER_PRESSURE_UNIT_MM_HG,  /* millimeters of mercury */
    GWEATHER_PRESSURE_UNIT_INCH_HG, /* inches of mercury */
    GWEATHER_PRESSURE_UNIT_ATM     /* atmosphere */
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
 *                                                  was starting a sentence
 * @GWEATHER_FORMAT_OPTION_NO_CAPITALIZATION: Capitalize as if the string was
 *                                            appearing within a sentence
 *
 * Format options to influence the returned string of the
 * gweather_*_to_string_full() functions.
 */
typedef enum { /*< underscore_name=gweather_format_options >*/
    GWEATHER_FORMAT_OPTION_DEFAULT                 = 0,
    GWEATHER_FORMAT_OPTION_SENTENCE_CAPITALIZATION = 1 << 0,
    GWEATHER_FORMAT_OPTION_NO_CAPITALIZATION       = 1 << 1
} GWeatherFormatOptions;

#endif /* __GWEATHER_ENUMS_H_ */
