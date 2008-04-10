EXTRA_DIST =			\
	check-timezones.sh	\
	locations_diff.py	\
	locations.dtd		\
	Locations.xml           \
	Locations.xml.in	\
	README

libgweatherlocationsdir = $(pkgdatadir)
libgweatherlocations_in_files = Locations.xml.in
dist_libgweatherlocations_DATA = $(libgweatherlocations_in_files:.xml.in=.xml) locations.dtd
%.xml: %.xml.in $(wildcard $(top_srcdir)/po-locations/*.po)
	LC_ALL=C $(INTLTOOL_MERGE) -x -u -c $(top_builddir)/po-locations/.intltool-merge-cache $(top_srcdir)/po-locations $< $@

check:
	xmllint --valid --noout $(srcdir)/Locations.xml.in && \
	$(srcdir)/check-timezones.sh $(srcdir)/Locations.xml.in
