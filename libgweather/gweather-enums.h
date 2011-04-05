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

typedef enum { /*< underscore_name=gweather_forecast_type >*/
    GWEATHER_FORECAST_STATE,
    GWEATHER_FORECAST_ZONE,
    GWEATHER_FORECAST_LIST
} GWeatherForecastType;

typedef enum { /*< underscore_name=gweather_temperature_unit >*/
    GWEATHER_TEMP_UNIT_INVALID = 0,
    GWEATHER_TEMP_UNIT_DEFAULT,
    GWEATHER_TEMP_UNIT_KELVIN,
    GWEATHER_TEMP_UNIT_CENTIGRADE,
    GWEATHER_TEMP_UNIT_FAHRENHEIT
} GWeatherTemperatureUnit;

typedef enum { /*< underscore_name=gweather_speed_unit >*/
    GWEATHER_SPEED_UNIT_INVALID = 0,
    GWEATHER_SPEED_UNIT_DEFAULT,
    GWEATHER_SPEED_UNIT_MS,    /* metres per second */
    GWEATHER_SPEED_UNIT_KPH,   /* kilometres per hour */
    GWEATHER_SPEED_UNIT_MPH,   /* miles per hour */
    GWEATHER_SPEED_UNIT_KNOTS, /* Knots */
    GWEATHER_SPEED_UNIT_BFT    /* Beaufort scale */
} GWeatherSpeedUnit;

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

typedef enum { /*< underscore_name=gweather_distance_unit >*/
    GWEATHER_DISTANCE_UNIT_INVALID = 0,
    GWEATHER_DISTANCE_UNIT_DEFAULT,
    GWEATHER_DISTANCE_UNIT_METERS,
    GWEATHER_DISTANCE_UNIT_KM,
    GWEATHER_DISTANCE_UNIT_MILES
} GWeatherDistanceUnit;

#endif /* __GWEATHER_ENUMS_H_ */
