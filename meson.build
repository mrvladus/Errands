project(
  'errands',
  'c',
  license: 'MIT',
  version: '47.0',
)

# Dependencies
dependencies = [
  dependency('libadwaita-1', version: '>=1.6'),
  dependency('gtk4'),
  dependency('libportal'),
  dependency('libical'),
  dependency('gtksourceview-5'),
  dependency('json-glib-1.0'),
  dependency('libcurl'),
  dependency('libxml-2.0'),
  dependency('webkitgtk-6.0'),
  dependency('libcmark'),
]

# Source files
sources = files(
  'src/attachments-window.c',
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
