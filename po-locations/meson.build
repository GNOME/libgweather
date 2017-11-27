# XXX: This should be a files() object in data/ but i18n.gettext() does not
# like that with Meson 0.43
locations_its = join_paths(meson.current_source_dir(), '../data/locations.its')
i18n.gettext('libgweather-locations',
             args: ['--its', locations_its])
