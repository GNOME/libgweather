Title: Adding new locations

# Adding new locations

## What are locations

Libgweather contains information about:

- major cities in each country
- timezones for those cities as well as countries
- [METAR](https://en.wikipedia.org/wiki/METAR) weather stations, attached to
  airports

For cities which aren't in the list, and if you're connected to the Internet,
libgweather also uses OpenStreetMap's Nominatim service to search for
locations.

The locations are stored inside the `Locations.xml` file in the
[gweather-locations
repository](https://gitlab.gnome.org/GNOME/gweather-locations). Always check
whether your city, METAR weather station/airport, or timezone is already in
there.

## What do we want in libgweather

Ideally, libgweather would contain a list of all the more important cities in
each country, with a METAR weather station/airport near it listed, so that
accurate current weather can be fetched, and timezone or world clocks
configuration can be done offline, without the need to resort to online
databases.

If you live in a specific country, you can probably better understand the
expectations of users of that country when it comes to having their nearest
"big city" listed. We usually prefer to only list cities with at least 100
thousand inhabitants, but it's not a hard limit when it makes sense.

Note that METAR weather stations/airports aren't the only weather stations that
we know how to query, so don't worry if you can't find one.

## Contributing

You've already looked in the `Locations.xml` file, and in the project's
[open issues](https://gitlab.gnome.org/GNOME/gweather-locations/issues) and
[open merge requests](https://gitlab.gnome.org/GNOME/gweather-locations/merge_requests),
and the timezone is incorrect, the weather can't be fetched, or your city isn't
listed.

First, try to find if something changed. For example, the country's timezone
changed recently, or an airport closed, and collect those references. Our go-to
reference is usually Wikipedia, because it usually references sources either in
English or other languages.

Possible references are:

- [List of timezones](https://en.wikipedia.org/wiki/List_of_tz_database_time_zones)
- [List of airports by ICAO code](https://en.wikipedia.org/wiki/ICAO_airport_code)
- [List of FIPS country codes](https://en.wikipedia.org/wiki/List_of_FIPS_country_codes)
- [List of ISO country codes](https://en.wikipedia.org/wiki/ISO_3166-1#Current_codes)

All done? Onto the next part

### I don't know XML, I don't have time

Simply tell us about the problem, and give us as much information as possible
in [a new issue](https://gitlab.gnome.org/GNOME/gweather-locations/-/issues/). Make
sure to include links to explanation if at all possible. It's easier for you to
know that a city is misnamed if you live there, but we want to be sure we're
not making mistakes, because that would be quite frustrating.

### I know XML at least a little

If you know git, you probably know how to contribute Merge Requests and all
that, so carry on to the next section.

For the others, open the text file editor. Once you've made your changes to the
file, you'll need to change:

- the first line of the commit message to "locations: [summary of the changes]"
- on the subsequent lines, add more explanation if any is needed, and all the
  links to justify your change
- choose a descriptive name for the "Branch name". We usually prefer lower-case
  names with no spaces.

And click "Commit changes". Congratulations on making libgweather better!

For more information and examples, you can also check the [GNOME
Handbook](https://handbook.gnome.org).

### Potential problems with changes

We run a battery of automated tests on changes made to the `Locations.xml`
file, to avoid problems on the more than 8000 locations it contains. Here's a
list of potential problems we might run into:

- the city is too small. If it has less than 100 thousand inhabitants, we might
  want to accept it anyway if it is the capital of a state/region in the
  country, and no bigger cities are around that people would usually search
  for.
- the airport is too far from the city. We have the coordinates for both the
  city and a number of airports. If there's no airport listed, we'll try to
  find the closest one in the same country. This is useful if you have 2 big
  cities close to each other, but a shared airport.
- the METAR weather station doesn't transmit data to the weather service we
  use. Don't worry about it, include the airport, and we'll figure it out.
- the coordinates are invalid. libgweather expects the coordinates to be
  decimal, with a space separating them. On most Wikipedia pages, there will be
  a link to the GeoHack website which will have the decimal value. Don't
  forget to remove the "," and make sure you copied the "-" signs if they're
  there!

Other than that, if in doubt, include the information you're unsure about, and
look around in the file for examples.

## Further maintenance

Want to help more? Pick a country, one you know well would be best, and look at
the definitions.

- Is every city name correct? (ie, it's the real name of a real city which is
  actually in that country, not some other country.)
- For cities known by multiple names:
  - If the city is known by a different name in English than it is locally, is
    it listed by its English name, with a comment indicating the local name?
  - If the city is known by multiple names locally, are there comments
    indicating the other local names? (Note that we're mostly only interested
    in local names; it's generally not interesting to have comments giving
    alternative foreign names for the city, except in the case of cities like
    Gdańsk/Danzig or Kaliningrad/Königsberg that have very conspicuously
    changed names over the centuries.)
  - For cities with multiple local names but no English-specific name, are we
    using the right local name as the "English" version? (Eg, since our dataset
    has both Finnish and Swedish names for many cities in Finland, we have to
    explicitly tell it to use the Finnish names. There may be other countries
    that have this problem as well.)
- Likewise, for countries that are divided into `<state>`s, are the `<state>`
  names correct? (Basically all the same issues as the city names above apply
  here.)
  - Also, are all cities in the correct `<state>`?
  - One known issue with the handling of `<state>`s: we can't currently deal
    with cities that are considered to be their own state-level entities (eg,
    Beijing, China or Berlin, Germany); For now, those cities need to be placed
    inside imaginary `<state>`s of the same name. This will eventually be
    fixed.
- Is the use of transliteration / diacritic marks correct and "good"? In some
  cases we're stripping out diacritic marks to make the names more
  English-like. (Eg, the source dataset has all sorts of macrons and hooks and
  stuff in its transliterations of Arabic names, so that "Riyadh" beomes
  "Riyāḑ", etc. We flatten those back down to ASCII.) It might be the case that
  in some countries we're stripping out diacritics that we should be leaving
  there. (Or vice versa, maybe we're leaving in diacritics that should be
  skipped in the English versions?)

And onto the harder stuff:

- Does each city and each METAR weather station location have the correct
  coordinates?
  - Cities are easy (but time-consuming) to verify; just search for the
    coordinates in Google Maps. (The coordinates listed don't need to be the
    exact coordinates of the city center, just something reasonably close.)
  - Weather stations are harder; if the weather station is an airport, you can
    often tell that the coordinates are correct because you can see the
    airport's runways in the Google Maps satellite view. Other than that, if
    the station is named after a city or geographic feature, you can at least
    check that it really is near that feature. (In particularly egregious
    cases, you can tell that the coordinates must be wrong because they place
    the weather station in the ocean, or in the wrong country...)
