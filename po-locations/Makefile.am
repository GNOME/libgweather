EXTRA_DIST = ChangeLog \
	$(wildcard *.po) \
	extract.xsl

CLEANFILES = locations.pot Locations.xml.in.h

DISTCLEANFILES = .intltool-merge-cache

Locations.xml.in.h: $(top_srcdir)/data/Locations.xml.in extract.xsl
	xsltproc extract.xsl $(top_srcdir)/data/Locations.xml.in > $@

locations.pot: Locations.xml.in.h
	$(XGETTEXT) --add-comments --output=$@ --from-code=utf-8 --keyword=N_ --keyword=NC_:1c,2 --no-location $^

%.po: locations.pot FORCE
	$(MSGMERGE) -U $@ locations.pot

FORCE:
