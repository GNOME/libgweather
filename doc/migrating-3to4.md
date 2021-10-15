Title: Migrating from GWeather 3.x to GWeather 4

## Migrating from GWeather 3.x to GWeather 4

GWeather 4 is a new major version that breaks API and ABI compared with
GWeather 3.

### Stop using `GWeatherLocationEntry` and `GWeatherTimezoneMenu`

GWeather 4 does not provide GTK widgets for selecting a location or a time
zone. Applications should provide their own UI, if needed, according to the
best practices of the [GNOME human interface
guidelines](https://developer.gnome.org/hig/).

### Stop using `gweather_location_get_children()`

In order to iterate over the child locations of a [struct@GWeather.Location],
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
reference to it, using [method@GWeather.Location.ref].
