project(
  'errands',
  'c',
  license: 'MIT',
  version: '47.1',
)

# Dependencies
dependencies = [
  dependency('gtk4', version: '>=4.16'),
  dependency('libadwaita-1', version: '>=1.6'),
  dependency('libportal', version: '>=0.8'),
  dependency('libical', version: '>=3.0'),
]

# Source files
sources = files(
  'src' / 'data.c',
  'src' / 'lib' / 'cJSON.c',
  'src' / 'main.c',
  'src' / 'no-lists-page.c',
  'src' / 'new-list-dialog.c',
  'src' / 'resources.c',
  'src' / 'settings.c',
  'src' / 'sidebar-all-row.c',
  'src' / 'sidebar-task-list-row.c',
  'src' / 'sidebar.c',
  'src' / 'state.c',
  'src' / 'task-list.c',
  'src' / 'task.c',
  'src' / 'window.c',
)

# Configuration config.h file
configure_file(
  input: 'src' / 'config.h.in',
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
