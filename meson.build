project(
  'errands',
  'c',
  license: 'MIT',
  version: '48.0',
)

# Dependencies
dependencies = [
  dependency('libportal'), # For flatpak sandbox
  dependency('libical'), # For working with user data in ical format
  dependency('gtksourceview-5'), # For notes syntax highlighting
  dependency('webkitgtk-6.0'), # For markdown preview
  dependency('libcmark'), # For markdown preview
  dependency('libcurl'), # For caldav requests
  dependency('json-glib-1.0'), # For parsing json
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>=1.6'),
]

# Source files
sources = files(
  'src/attachments-window.c',
  'src/caldav.c',
  'src/color-window.c',
  'src/data.c',
  'src/date-chooser.c',
  'src/date-window.c',
  'src/delete-list-dialog.c',
  'src/main.c',
  'src/month-chooser.c',
  'src/new-list-dialog.c',
  'src/no-lists-page.c',
  'src/notes-window.c',
  'src/priority-window.c',
  'src/rename-list-dialog.c',
  'src/settings.c',
  'src/sidebar-all-row.c',
  'src/sidebar-task-list-row.c',
  'src/sidebar.c',
  'src/state.c',
  'src/sync.c',
  'src/tags-window.c',
  'src/task-list.c',
  'src/task-toolbar.c',
  'src/task.c',
  'src/time-chooser.c',
  'src/week-chooser.c',
  'src/window.c',
)

# Configuration config.h file
configure_file(
  input: 'src/config.h.in',
  output: 'config.h',
  configuration: {
    'VERSION': meson.project_version(),
  },
)

# Resources
gnome = import('gnome')
resources = gnome.compile_resources(
  meson.project_name(),
  'data/errands.gresource.xml',
  c_name: meson.project_name(),
  source_dir: 'data',
)

# Errands executable
bin = executable(
  meson.project_name(),
  sources: [sources, resources],
  dependencies: dependencies,
)

# Run target for building and running errands: 'meson compile run'
run_target('run', command: bin)
