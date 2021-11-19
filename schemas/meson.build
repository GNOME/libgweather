gsettingsdir = datadir / 'glib-2.0/schemas'
install_data('org.gnome.GWeather4.gschema.xml',
  install_dir: gsettingsdir,
)

schemas_enums = gnome.mkenums('org.gnome.GWeather4.enums.xml',
  comments: '<!-- @comment@ -->',
  fhead: '<schemalist>',
  vhead: '<@type@ id="org.gnome.GWeather4.@EnumName@">',
  vprod: '<value nick="@valuenick@" value="@valuenum@"/>',
  vtail: '</@type@>',
  ftail: '</schemalist>',
  sources: ['../libgweather/gweather-enums.h'],
  install_header: true,
  install_dir: gsettingsdir,
)
