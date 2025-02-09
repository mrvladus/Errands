project(
  'errands',
  'c',
  license: 'MIT',
  version: '47.0',
)

# Dependencies
dependencies = [
  dependency('gtk4', version: '>=4.16'),
  dependency('libadwaita-1', version: '>=1.6'),
  dependency('libportal', version: '>=0.8'),
  dependency('libsecret-1', version: '>=0.21'),
  dependency('libical', version: '>=3.0'),
  dependency('gtksourceview-5', version: '>=5.14'),
  dependency('libecal-2.0', version: '>=3.54'),
  dependency('json-glib-1.0', version: '>=1.10'),
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
  'src/resources.c',
  'src/settings.c',
  'src/sidebar-all-row.c',
  'src/sidebar-task-list-row.c',
  'src/sidebar.c',
  'src/state.c',
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

# Errands executable
bin = executable(
  meson.project_name(),
  sources: sources,
  dependencies: dependencies,
)

# Run target for building and running errands: 'meson compile run'
run_target('run', command: bin)
