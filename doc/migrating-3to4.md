Title: Migrating from GWeather 3.x to GWeather 4

## Migrating from GWeather 3.x to GWeather 4

GWeather 4 is a new major version that breaks API and ABI compared with
GWeather 3.

### Stop using `GWeatherLocationEntry` and `GWeatherTimezoneMenu`

GWeather 4 does not provide GTK widgets for selecting a location or a time
zone. Applications should provide their own UI, if needed, according to the
best practices of the [GNOME human interface
guidelines](https://developer.gnome.org/hig/).
