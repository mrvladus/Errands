configure_file(
  input: 'errands.py',
  output: 'errands',
  configuration: conf,
  install: true,
  install_dir: get_option('bindir'),
  install_mode: 'r-xr--r--'
)

install_subdir('utils', install_dir: moduledir)
install_subdir('lib', install_dir: moduledir)
install_subdir('widgets', install_dir: moduledir)

if profile == 'development'
  install_subdir('tests', install_dir: moduledir)
endif
