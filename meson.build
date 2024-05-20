project(
  'errands',
  version: '46.2.1',
  meson_version: '>= 0.62.0',
)

# Declare dependencies
dependency('libadwaita-1', version: '>= 1.5')
dependency('libsecret-1')
dependency('libportal')
dependency('gtksourceview-5')
dependency('pygobject-3.0')
dependency('goa-1.0', required: false)

# Import modules
i18n = import('i18n')
gnome = import('gnome')
python = import('python')

# Define dirs
pkgdatadir = join_paths(get_option('prefix'), get_option('datadir'), meson.project_name())
moduledir = join_paths(pkgdatadir, 'errands')

# Set profile
profile = get_option('profile')
if profile == 'development'
  app_id = 'io.github.mrvladus.List.Devel'
  prefix = '/io/github/mrvladus/List/Devel'
elif profile == 'release'
  app_id = 'io.github.mrvladus.List'
  prefix = '/io/github/mrvladus/List'
endif

# Set config
conf = configuration_data()
conf.set('PYTHON', python.find_installation('python3').full_path())
conf.set('PYTHON_VERSION', python.find_installation('python3').language_version())
conf.set('APP_ID', app_id)
conf.set('PREFIX', prefix)
conf.set('VERSION', meson.project_version())
conf.set('PROFILE', profile)
conf.set('localedir', join_paths(get_option('prefix'), get_option('localedir')))
conf.set('pkgdatadir', pkgdatadir)
conf.set('bindir', get_option('prefix') / get_option('bindir'))

# Install data
subdir('data')

# Install source files
configure_file(
  input: join_paths('errands', 'errands.py'),
  output: 'errands',
  configuration: conf,
  install: true,
  install_dir: get_option('bindir'),
  install_mode: 'r-xr--r--',
)
install_data(join_paths('errands', 'application.py'), install_dir: moduledir)
install_data(join_paths('errands', 'state.py'), install_dir: moduledir)
install_subdir(join_paths('errands', 'utils'), install_dir: moduledir)
install_subdir(join_paths('errands', 'lib'), install_dir: moduledir)
install_subdir(join_paths('errands', 'widgets'), install_dir: moduledir)

# Install translations
subdir('po')

# Update cache, icons and compile gsettings
gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)