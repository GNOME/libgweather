
version_conf = configuration_data()
version_conf.set('LIBGWEATHER_VERSION', meson.project_version())
configure_file(input: 'version.xml.in', output: 'version.xml', configuration: version_conf)

glib_prefix = dependency('glib-2.0').get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')

gtk_prefix = dependency('gtk+-3.0').get_pkgconfig_variable('prefix')
gtk_docpath = join_paths(gtk_prefix, 'share', 'gtk-doc', 'html')

docpath = join_paths(datadir, 'gtk-doc', 'html')

gnome.gtkdoc('libgweather',
  main_xml: 'libgweather-docs.xml',
  src_dir: ['libgweather'],
  dependencies: libgweather_dep,
  gobject_typesfile: [join_paths(meson.current_source_dir(), 'libgweather.types')],
  ignore_headers: [
    'gweather-enum-types.h',
    'gweather-parser.h',
    'gweather-private.h',
  ],
  mkdb_args: ['--xml-mode', '--output-format=xml'],
  scan_args: [
    '--deprecated-guards="GWEATHER_DISABLE_DEPRECATED"',
    '--ignore-decorators=GWEATHER_EXTERN',
  ],
  fixxref_args: [
    '--html-dir=@0@'.format(docpath),
    '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
    '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
    '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
    '--extra-dir=@0@'.format(join_paths(gtk_docpath, 'gtk')),
  ],
  install: true,
)
