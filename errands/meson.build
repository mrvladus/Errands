gnome.compile_resources('errands',
  join_paths('resources', 'errands.gresource.xml'),
  gresource_bundle: true,
  install: true,
  install_dir: pkgdatadir,
)


configure_file(
  input: 'errands.py',
  output: 'errands',
  configuration: conf,
  install: true,
  install_dir: get_option('bindir'),
  install_mode: 'r-xr--r--'
)

install_subdir('utils', install_dir: moduledir)
install_subdir('widgets', install_dir: moduledir)
