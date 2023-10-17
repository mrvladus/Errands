subdir('res')

configure_file(
  input: 'errands.py',
  output: 'errands',
  configuration: conf,
  install: true,
  install_dir: get_option('bindir'),
  install_mode: 'r-xr--r--'
)

install_data(
  [
  'application.py',
  'preferences.py',
  'sync.py',
  'task.py',
  'task_details.py',
  'trash_item.py',
  'utils.py',
  'window.py',
  ], 
  install_dir: join_paths(pkgdatadir, meson.project_name())
)
