subdir('resources')

install_subdir('icons', install_dir: get_option('datadir'))

desktop_file = i18n.merge_file(
  input: configure_file(
    input: 'io.github.mrvladus.List.desktop.in.in',
    output: app_id + '.desktop.in',
    configuration: conf,
  ),
  output: app_id + '.desktop',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils, args: [desktop_file])
endif

appstream_file = i18n.merge_file(
  input: configure_file(
    input: 'io.github.mrvladus.List.metainfo.xml.in.in',
    output: app_id + '.metainfo.xml.in',
    configuration: conf,
  ),
  output: app_id + '.metainfo.xml',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'metainfo'),
)

# Validate Appdata
appstreamcli = find_program('appstreamcli', required: false)
if (appstreamcli.found())
  test(
    'validate-appdata',
    appstreamcli,
    args: ['validate', '--no-net', '--explain', appstream_file],
    workdir: meson.current_build_dir(),
  )
endif

install_data(
  configure_file(
    input: 'io.github.mrvladus.List.gschema.xml.in',
    output: app_id + '.gschema.xml',
    configuration: conf,
  ),
  install_dir: join_paths(get_option('datadir'), 'glib-2.0/schemas'),
)

compile_schemas = find_program('glib-compile-schemas', required: false)
if compile_schemas.found()
  test(
    'Validate schema file',
    compile_schemas,
    args: ['--strict', '--dry-run', meson.current_source_dir()],
  )
endif

configure_file(
  input: 'io.github.mrvladus.List.service.in',
  output: '@0@.service'.format(app_id),
  configuration: conf,
  install: true,
  install_dir: get_option('datadir') / 'dbus-1/services',
)