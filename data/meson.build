install_data(
  join_paths('icons', 'io.github.mrvladus.List.svg'),
  install_dir: join_paths(get_option('datadir'), 'icons', join_paths('hicolor', 'scalable', 'apps'))
)

install_data(
  join_paths('icons', 'io.github.mrvladus.List-symbolic.svg'),
  install_dir: join_paths(get_option('datadir'), 'icons', join_paths('hicolor', 'symbolic', 'apps'))
)

install_data(
  'io.github.mrvladus.List.desktop',
  install_dir: join_paths(get_option('datadir'), 'applications')
)

install_data(
  'io.github.mrvladus.List.metainfo.xml',
  install_dir: join_paths(get_option('datadir'), 'metainfo')
)

install_data(
  configure_file(
    input: 'io.github.mrvladus.List.gschema.xml',
    output: app_id + '.gschema.xml',
    configuration: conf
  ),
  install_dir: join_paths(get_option('datadir'), 'glib-2.0/schemas')
)

compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  test('Validate schema file',
       compile_schemas,
       args: ['--strict', '--dry-run', meson.current_source_dir()])
endif