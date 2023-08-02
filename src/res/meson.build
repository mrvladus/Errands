blueprints = custom_target('blueprints',
  input: files(
    'ui/preferences.blp',
    'ui/sub_task.blp',
    'ui/task.blp',
    'ui/window.blp',
    'ui/trash_item.blp',
  ),
  output: '.',
  command: [find_program('blueprint-compiler'), 'batch-compile', '@OUTPUT@', '@CURRENT_SOURCE_DIR@', '@INPUT@'],
)

gnome.compile_resources('io.github.mrvladus.List',
  'io.github.mrvladus.List.gresource.xml',
  dependencies: blueprints,
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir,
)
