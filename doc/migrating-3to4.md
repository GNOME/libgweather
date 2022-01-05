Title: Migrating from GWeather 3.x to GWeather 4

# Migrating from GWeather 3.x to GWeather 4

GWeather 4 is a new major version that breaks API and ABI compared with
GWeather 3.

### Stop using `GWeatherLocationEntry` and `GWeatherTimezoneMenu`

GWeather 4 does not provide GTK widgets for selecting a location or a time
zone. Applications should provide their own UI, if needed, according to the
best practices of the [GNOME human interface
guidelines](https://developer.gnome.org/hig/).

## Preparations

Before migrating to GWeather 4, there are steps to follow in order to minimise
the changes you have to implement once you switch.

### Stop using `gweather_location_get_children()`

In order to iterate over the child locations of a [class@GWeather.Location],
you should use the iterator method [`method@GWeather.Location.next_child`]:

```c
/* Iterating using get_children() */
GWeatherLocation **children = gweather_location_get_children (location);
for (guint i = 0; children[i] != NULL; i++) {
  GWeatherLocation *iter = children[i];
  // ...
}

/* Iterating using next_child() */
g_autoptr (GWeatherLocation) iter = NULL;
while ((iter = gweather_location_next_child (location, iter)) != NULL) {
  // ...
}
```

**Note**: Unlike `gweather_location_get_children()`, the `next_child()`
method will consume the reference of the iterated child; if you are keeping
a reference to each child `GWeatherLocation` you should acquire a strong
reference to it, using [method@GObject.Object.ref].

### Stop using `gweather_info_get_radar()`

The radar image provider stopped working a while ago, and the `get_radar()`
method has been returning `NULL` since then.

### Stop using `GWEATHER_PROVIDER_YAHOO`

The Yahoo! provider was removed in libgweather 3.28.

### Always pass a location to find and detect nearest location API

The [`method@GWeather.Location.find_nearest_city`],
[`method@GWeather.Location.find_nearest_city_full`], and
[`method@GWeather.Location.detect_nearest_city`] methods do not accept
`NULL` as the instance argument any more.

## Changes

The following changes in GWeather 4 are incompatible with GWeather 3.x, and
must be performed at the time of the port.

### `GWeatherLocation` is a GObject

[class@GWeather.Location] has been promoted to a full `GObject` type. This
means that properties using `GWeatherLocation` should be defined using
[`func@GObject.param_spec_object`], and `GValue`s should be accessed using
[`method@GObject.Value.set_object`] and [`method@GObject.Value.get_object`].
If you are using `gweather_location_ref()` to acquire a reference on a location
instance, you should now use [`method@GObject.Object.ref`]; if you are using
`gweather_location_unref()` to release a reference on a location instance,
you should now use [`method@GObject.Object.unref`].

### Use `GTimeZone` instead of `GWeatherTimezone`

The `GWeatherTimezone` type has been removed, in favor of the existing
`GTimeZone` type provided by GLib.

The [struct@GLib.TimeZone] type provides all the functionality of the
`GWeatherTimezone` structure, and includes all the interval data from
the time zone database.

If you are using `gweather_timezone_get_offset()` to compare two time
zones, you can replace that with [`method@GLib.TimeZone.find_interval`]
to find the interval relative to the current time; and then use
[`method@GLib.TimeZone.get_offset`] to retrieve the offset. For instance,
this code:

```c
GWeatherTimezone *tz_a = gweather_location_get_timezone (location_a);
GWeatherTimezone *tz_b = gweather_location_get_timezone (location_b);

int offset_a = gweather_timezone_get_offset (tz_a);
int offset_b = gweather_timezone_get_offset (tz_b);
```

can be rewritten as:

```c
GTimeZone *tz_a = gweather_location_get_timezone (location_a);
GTimeZone *tz_b = gweather_location_get_timezone (location_b);

// Find the interval relative to the current time; if you are performing
// the offset comparison in a loop, for instance to sort a list of time
// zones, you may want to hoist the computation of the "now" outside the
// loop, to ensure that the intervals are all relative to the same time
g_autoptr (GDateTime) dt = g_date_time_new_now_local ();
gint64 now = g_date_time_to_unix (dt);

int interval_a = g_time_zone_find_interval (tz_a, G_TIME_TYPE_STANDARD, now);
int interval_b = g_time_zone_find_interval (tz_b, G_TIME_TYPE_STANDARD, now);

// Retrieve the offset for the given interval
int offset_a = g_time_zone_get_offset (tz_a, interval_a);
int offset_b = g_time_zone_get_offset (tz_b, interval_b);
```
