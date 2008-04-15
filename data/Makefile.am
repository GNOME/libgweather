libgweatherdtddir = $(pkgdatadir)
libgweatherdtd_DATA = locations.dtd

libgweatherlocationsdir = $(pkgdatadir)
libgweatherlocations_in_files = Locations.xml.in

if USE_ONE_BIG_XML

libgweatherlocations_DATA = $(libgweatherlocations_in_files:.xml.in=.xml)

%.xml: %.xml.in $(wildcard $(top_srcdir)/po-locations/*.po)
	LC_ALL=C $(INTLTOOL_MERGE) -x -u -c $(top_builddir)/po-locations/.intltool-merge-cache $(top_srcdir)/po-locations $< $@

else # USE_ONE_BIG_XML

PO_LOCATIONS = $(wildcard $(top_srcdir)/po-locations/*.po)

# Helper variable
libgweatherlocations_data = $(libgweatherlocations_in_files:.xml.in=.xml)

libgweatherlocations_DATA = $(shell echo $(PO_LOCATIONS) | sed "s|$(top_srcdir)/po-locations/|Locations.|g;s|\.po|.xml|g") $(libgweatherlocations_data)

$(libgweatherlocations_DATA): $(libgweatherlocations_in_files) $(PO_LOCATIONS)
	LC_ALL=C $(INTLTOOL_MERGE) --multiple-output --xml-style --utf8 --cache=$(top_builddir)/po-locations/.intltool-merge-cache $(top_srcdir)/po-locations $< $(libgweatherlocations_data)
	for pofile in $(PO_LOCATIONS); do \
		locale=`echo $$pofile | sed "s;$(top_srcdir)/po-locations/\(.*\)\.po;\1;"`;	\
		xmllint --noblanks -o Locations.$$locale.xml $$locale/$(libgweatherlocations_data); \
		rm -f $$locale/$(libgweatherlocations_data);		\
		test -d $$locale && rmdir $$locale;			\
	done
	xmllint --noblanks -o Locations.xml C/$(libgweatherlocations_data)
	rm -f C/$(libgweatherlocations_data)
	test -d C && rmdir C

endif # USE_ONE_BIG_XML

check:
	xmllint --valid --noout $(top_srcdir)/data/Locations.xml.in
	$(srcdir)/check-timezones.sh $(srcdir)/Locations.xml.in

EXTRA_DIST =	\
	$(libgweatherlocations_in_files)	\
	$(libgweatherdtd_DATA)			\
	check-timezones.sh			\
	README

CLEANFILES =	\
	$(libgweatherlocations_DATA)
