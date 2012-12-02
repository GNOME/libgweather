gsettings_ENUM_NAMESPACE = org.gnome.GWeather
gsettings_ENUM_FILES = $(top_srcdir)/libgweather/gweather-enums.h

gsettings_SCHEMAS = org.gnome.GWeather.gschema.xml

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(gsettings_SCHEMAS)

CLEANFILES = \
	*.valid

-include $(top_srcdir)/git.mk
