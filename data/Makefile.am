
SUBDIRS =

if ENABLE_GLADE_CATALOG
SUBDIRS += glade
endif

libgweatherdtddir = $(pkgdatadir)
libgweatherdtd_DATA = locations.dtd

libgweatherlocationsdir = $(pkgdatadir)
libgweatherlocations_DATA = Locations.xml

### Locations.xml rebuild
rebuild-locations: locationdb.sqlite update-locations.py
	$(AM_V_GEN)($(srcdir)/update-locations.py > Locations.xml.new && mv Locations.xml.new Locations.xml) || rm -f Locations.xml.new

locationdb.sqlite: build-locationdb.pl major-cities.txt sources/nsd_cccc.txt sources/POP_PLACES.txt sources/US_CONCISE.txt sources/geonames_dd_dms_date_*.txt city-fixups.pl station-fixups.pl
	$(AM_V_GEN)$(srcdir)/build-locationdb.pl

check:
	xmllint --valid --noout $(top_srcdir)/data/Locations.xml
	$(srcdir)/check-timezones.sh $(srcdir)/Locations.xml

@INTLTOOL_XML_NOMERGE_RULE@

EXTRA_DIST =	\
	Locations.xml.in			\
	$(libgweatherdtd_DATA)			\
	check-timezones.sh			\
	README					\
	README.timezones

CLEANFILES = $(libgweatherlocations_DATA)

-include $(top_srcdir)/git.mk
