project(
  'errands',
  'c',
  license: 'MIT',
  version: '48.0',
)

# Dependencies
subdir('src' / 'sync' / 'caldav') # Build 'libcaldav' to work with CalDAV servers
dependencies = [
  dependency('libportal'), # For flatpak sandbox
  dependency('libical'), # For working with user data in ical format
  dependency('gtksourceview-5'), # For notes syntax highlighting
  dependency('webkitgtk-6.0'), # For markdown preview
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>=1.7'),
]

# Source files
sources = files(
  'src/data/data-io.c',
  'src/data/data-props.c',
  'src/data/data.c',
  'src/main.c',
  'src/no-lists-page.c',
  'src/settings.c',
  'src/sidebar/delete-list-dialog.c',
  'src/sidebar/new-list-dialog.c',
  'src/sidebar/rename-list-dialog.c',
  'src/sidebar/sidebar-all-row.c',
  'src/sidebar/sidebar-task-list-row.c',
  'src/sidebar/sidebar.c',
  'src/state.c',
  'src/sync/sync.c',
  'src/task-list/sort-dialog.c',
  'src/task-list/task-list.c',
  'src/task/attachments-window.c',
  'src/task/color-window.c',
  'src/task/date-chooser.c',
  'src/task/date-window.c',
  'src/task/month-chooser.c',
  'src/task/notes-window.c',
  'src/task/priority-window.c',
  'src/task/tags-window.c',
  'src/task/task.c',
  'src/task/time-chooser.c',
  'src/task/week-chooser.c',
  'src/window.c',
)

# Configuration config.h file
configure_file(
  input: 'src' / 'config.h.in',
  output: 'config.h',
  configuration: {
    'VERSION': meson.project_version(),
  },
)

# Resources
gnome = import('gnome')
resources = gnome.compile_resources(
  meson.project_name(),
  'data' / 'errands.gresource.xml',
  c_name: meson.project_name(),
  source_dir: 'data',
)

# Errands executable
bin = executable(
  meson.project_name(),
  sources: [sources, resources],
  dependencies: dependencies,
  link_with: libcaldav,
)

# Run target for building and running errands: 'meson compile run'
run_target('run', command: bin)
