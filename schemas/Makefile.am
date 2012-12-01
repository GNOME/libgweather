gschemas_in = org.gnome.GWeather.gschema.xml.in

gsettings_ENUM_NAMESPACE = org.gnome.GWeather
gsettings_ENUM_FILES = $(top_srcdir)/libgweather/gweather-enums.h

gsettings_SCHEMAS = $(gschemas_in:.xml.in=.xml)

@GSETTINGS_RULES@
@INTLTOOL_XML_NOMERGE_RULE@

EXTRA_DIST = $(gschemas_in)
CLEANFILES = $(gsettings_SCHEMAS) *.valid

-include $(top_srcdir)/git.mk
