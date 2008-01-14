EXTRA_DIST =			\
	locations_diff.py	\
	locations.dtd		\
	Locations.xml

libgweatherlocationsdir = $(pkgdatadir)
libgweatherlocations_in_files = Locations.xml.in
libgweatherlocations_DATA = $(libgweatherlocations_in_files:.xml.in=.xml) locations.dtd
%.xml:       %.xml.in       $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po-locations/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -x -u -c $(top_builddir)/po-locations/.intltool-merge-cache $(top_srcdir)/po-locations $< $@
