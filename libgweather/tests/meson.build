test_cargs = [
  '-DTEST_LOCATIONS="@0@"'.format(locations_bin.full_path()),
  '-DSCHEMASDIR="@0@/schemas"'.format(meson.source_root()),
  '-DSCHEMAS_BUILDDIR="@0@/schemas"'.format(meson.build_root()),
]

test('test_libgweather',
  executable('test_libgweather',
    sources: ['test_libgweather.c'],
    c_args: test_cargs,
    dependencies: libgweather_static_dep,
    install: false,
  ),
  depends: [locations_bin],
)
