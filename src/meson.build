moduledir = join_paths(pkgdatadir, meson.project_name())

subdir('res')

configure_file(
  input: 'errands.py',
  output: 'errands',
  configuration: conf,
  install: true,
  install_dir: get_option('bindir'),
  install_mode: 'r-xr--r--'
)

list_sources = [
  'application.py',
  'preferences.py',
  'sub_task.py',
  'task.py',
  'theme_switcher.py',
  'utils.py',
  'window.py',
]

install_data(list_sources, install_dir: moduledir)
