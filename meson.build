project(
  'errands',
  version: '44.7.3',
  meson_version: '>= 0.62.0',
)

i18n = import('i18n')
gnome = import('gnome')
python = import('python')

pkgdatadir = join_paths(get_option('prefix'), get_option('datadir'), meson.project_name())

profile = get_option('profile')
if profile == 'development'
  app_id = 'io.github.mrvladus.List.Devel'
  prefix = '/io/github/mrvladus/List/Devel'
elif profile == 'release'
  app_id = 'io.github.mrvladus.List'
  prefix = '/io/github/mrvladus/List'
endif

conf = configuration_data()
conf.set('PYTHON', python.find_installation('python3').full_path())
conf.set('PYTHON_VERSION', python.find_installation('python3').language_version())
conf.set('APP_ID', app_id)
conf.set('PREFIX', prefix)
conf.set('VERSION', meson.project_version())
conf.set('PROFILE', profile)
conf.set('localedir', join_paths(get_option('prefix'), get_option('localedir')))
conf.set('pkgdatadir', pkgdatadir)

subdir('data')
subdir('src')
subdir('po')

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)
