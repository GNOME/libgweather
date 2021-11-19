test_cargs = [
  '-DTEST_LOCATIONS="@0@"'.format(locations_bin.full_path()),
  '-DSCHEMASDIR="@0@/schemas"'.format(meson.source_root()),
  '-DSCHEMAS_BUILDDIR="@0@/schemas"'.format(meson.build_root()),
]

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('GIO_USE_VFS', 'local')
test_env.set('GSETTINGS_BACKED', 'memory')
test_env.set('G_ENABLE_DIAGNOSTIC', '0')
test_env.set('LIBGWEATHER_LOCATIONS_PATH', locations_bin.full_path())

test('test_libgweather',
  executable('test_libgweather',
    sources: ['test_libgweather.c'],
    c_args: test_cargs,
    dependencies: libgweather_static_dep,
    install: false,
  ),
  args: ['--tap', '-k'],
  protocol: 'tap',
  env: test_env,
  depends: [locations_bin],
)
