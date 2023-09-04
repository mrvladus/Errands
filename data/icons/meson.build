scalable_dir = join_paths('hicolor', 'scalable', 'apps')
install_data(
  ('@0@.svg').format(app_id),
  install_dir: join_paths(get_option('datadir'), 'icons', scalable_dir)
)

symbolic_dir = join_paths('hicolor', 'symbolic', 'apps')
install_data(
  'io.github.mrvladus.List-symbolic.svg',
  rename: ('@0@-symbolic.svg').format(app_id),
  install_dir: join_paths(get_option('datadir'), 'icons', symbolic_dir)
)