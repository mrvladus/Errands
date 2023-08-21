pkgdatadir = join_paths(get_option('prefix'), get_option('datadir'), meson.project_name())
moduledir = join_paths(pkgdatadir, 'list')

subdir('res')

python = import('python')

conf = configuration_data()
conf.set('PYTHON', python.find_installation('python3').full_path())
conf.set('VERSION', meson.project_version())
conf.set('APP_ID', 'io.github.mrvladus.List')
conf.set('localedir', join_paths(get_option('prefix'), get_option('localedir')))
conf.set('pkgdatadir', pkgdatadir)

configure_file(
  input: 'list.py',
  output: 'list',
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
