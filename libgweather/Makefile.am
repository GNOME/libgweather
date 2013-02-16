# we need to build two libraries (one real and one static)
# because we access private symbols from the tests
lib_LTLIBRARIES = libgweather-3.la
noinst_LTLIBRARIES = libgweather-internal-3.la
noinst_PROGRAMS = test_metar test_locations test_sun_moon

AM_CPPFLAGS = 			\
	$(GTK_CFLAGS)		\
	-I$(top_srcdir)		\
	-I$(srcdir)

AM_CFLAGS = $(WARN_CFLAGS)

gweather_new_headers = \
	gweather.h \
	gweather-location.h location-entry.h \
	gweather-timezone.h timezone-menu.h \
	gweather-weather.h gweather-enums.h

libgweatherincdir = $(includedir)/libgweather-3.0/libgweather
libgweatherinc_HEADERS = \
	$(gweather_new_headers)		\
	gweather-enum-types.h

libgweather_internal_3_la_SOURCES = \
	weather.c weather-priv.h \
	weather-metar.c weather-iwin.c weather-met.c \
	weather-bom.c weather-yahoo.c weather-wx.c \
	weather-yrno.c \
	weather-sun.c weather-moon.c \
	gweather-enum-types.c \
	gweather-location.c gweather-location.h \
	gweather-timezone.c gweather-timezone.h \
	location-entry.c location-entry.h \
	timezone-menu.c timezone-menu.h \
	parser.c parser.h

libgweather_internal_3_la_CPPFLAGS = \
	$(AM_CPPFLAGS)			\
	$(LIBXML_CFLAGS)		\
	$(LIBSOUP_CFLAGS)		\
	$(GIO_CFLAGS)			\
	-DG_LOG_DOMAIN=\"GWeather\"	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DGWEATHER_XML_LOCATION_DIR=\""$(pkgdatadir)"\"

libgweather_internal_3_la_LIBADD = \
	$(LIBM)		\
	$(GTK_LIBS)	\
	$(LIBXML_LIBS)	\
	$(LIBSOUP_LIBS)	\
	$(GIO_LIBS) \
	$(REGEX_LIBS)

libgweather_3_la_SOURCES = 
libgweather_3_la_LIBADD = libgweather-internal-3.la
libgweather_3_la_LDFLAGS = \
	-export-symbols-regex "^gweather_(.*)" \
	-version-info $(LT_VERSION) -no-undefined

test_metar_SOURCES = test_metar.c
test_metar_CPPFLAGS = $(AM_CPPFLAGS) $(LIBSOUP_CFLAGS)
test_metar_LDADD = libgweather-internal-3.la $(GTK_LIBS)

test_locations_SOURCES = test_locations.c
test_locations_LDADD = libgweather-internal-3.la $(GTK_LIBS)

test_sun_moon_SOURCES = test_sun_moon.c
test_sun_moon_CPPFLAGS = $(AM_CPPFLAGS) $(LIBSOUP_CFLAGS)
test_sun_moon_LDADD = libgweather-internal-3.la $(GTK_LIBS)

gweather-enum-types.h: $(gweather_new_headers)
	$(AM_V_GEN)( cd $(srcdir) && $(GLIB_MKENUMS) --template gweather-enum-types.h.tmpl \
		$(gweather_new_headers) ) > gweather-enum-types.h.tmp \
	&& mv gweather-enum-types.h.tmp gweather-enum-types.h \
	|| rm -f gweather-enum-type.h.tmp

gweather-enum-types.c: $(gweather_new_headers)
	$(AM_V_GEN)( cd $(srcdir) && $(GLIB_MKENUMS) --template gweather-enum-types.c.tmpl \
		$(gweather_new_headers) ) > gweather-enum-types.c.tmp \
	&& mv gweather-enum-types.c.tmp gweather-enum-types.c \
	|| rm -f gweather-enum-type.c.tmp

BUILT_SOURCES = gweather-enum-types.c gweather-enum-types.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gweather-3.0.pc

@INTLTOOL_SCHEMAS_RULE@

EXTRA_DIST = gweather-3.0.pc.in gweather-3.0-uninstalled.pc.in $(schema_in_files)

CLEANFILES = $(schema_DATA) $(noinst_PROGRAMS) $(BUILT_SOURCES)

# Introspection
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all --add-include-path=$(srcdir) --c-include=libgweather/gweather.h
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_sources = 	\
	weather.c gweather-weather.h \
	gweather-location.c gweather-location.h \
	gweather-timezone.c gweather-timezone.h \
	location-entry.c location-entry.h \
	timezone-menu.c timezone-menu.h

built_introspection_sources = gweather-enum-types.h gweather-enums.h

GWeather-3.0.gir: libgweather-3.la Makefile
GWeather_3_0_gir_INCLUDES = GObject-2.0 Gtk-3.0
GWeather_3_0_gir_CFLAGS = $(libgweather_internal_3_la_CPPFLAGS) -DGWEATHER_I_KNOW_THIS_IS_UNSTABLE
GWeather_3_0_gir_LIBS = libgweather-3.la
GWeather_3_0_gir_SCANNERFLAGS = --identifier-prefix=GWeather --symbol-prefix=gweather --pkg-export=gweather-3.0
GWeather_3_0_gir_FILES = $(addprefix $(srcdir)/,$(introspection_sources)) $(addprefix $(builddir)/,$(built_introspection_sources))
INTROSPECTION_GIRS += GWeather-3.0.gir

girdir = $(INTROSPECTION_GIRDIR)
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(INTROSPECTION_TYPELIBDIR)
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

-include $(top_srcdir)/git.mk
