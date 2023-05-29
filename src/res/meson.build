pkgdatadir = join_paths(get_option('prefix'), get_option('datadir'), meson.project_name())

blueprints = custom_target('blueprints',
  input: files(
    'ui/preferences.blp',
    'ui/sub_task.blp',
    'ui/task.blp',
    'ui/window.blp',
  ),
  output: '.',
  command: [find_program('blueprint-compiler'), 'batch-compile', '@OUTPUT@', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)


gnome = import('gnome')

gnome.compile_resources('io.github.mrvladus.List',
  'io.github.mrvladus.List.gresource.xml',
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir,
  dependencies: blueprints,
)
