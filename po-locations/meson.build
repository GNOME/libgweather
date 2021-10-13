# XXX: This should be a files() object in data/ but i18n.gettext() does not
# like that with Meson 0.43
locations_its = join_paths(meson.current_source_dir(), '../data/locations.its')
i18n.gettext(LOCATIONS_GETTEXT_PACKAGE, args: ['--its', locations_its])
