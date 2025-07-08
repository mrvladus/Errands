project(
  'errands',
  'c',
  license: 'MIT',
  version: '48.0',
)

dependencies = [
  dependency('libportal'), # For checking flatpak sandbox
  dependency('libical'), # For working with user data in ical format
  dependency('gtksourceview-5'), # For notes syntax highlighting
  dependency('webkitgtk-6.0'), # For markdown preview
  dependency('libcurl'),
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>=1.7'),
]

# Source files
sources = files(
  'src/data/data-io.c',
  'src/data/data-props.c',
  'src/data/data.c',
  'src/main.c',
  'src/settings.c',
  'src/sidebar-all-row.c',
  'src/sidebar-delete-list-dialog.c',
  'src/sidebar-new-list-dialog.c',
  'src/sidebar-rename-list-dialog.c',
  'src/sidebar-task-list-row.c',
  'src/sidebar.c',
  'src/state.c',
  'src/sync/caldav/caldav-calendar.c',
  'src/sync/caldav/caldav-client.c',
  'src/sync/caldav/caldav-event.c',
  'src/sync/caldav/caldav-list.c',
  'src/sync/caldav/caldav-requests.c',
  'src/sync/sync.c',
  'src/task-list-attachments-dialog-attachment.c',
  'src/task-list-attachments-dialog.c',
  'src/task-list-color-dialog.c',
  'src/task-list-date-dialog-date-chooser.c',
  'src/task-list-date-dialog-month-chooser.c',
  'src/task-list-date-dialog-rrule-row.c',
  'src/task-list-date-dialog-time-chooser.c',
  'src/task-list-date-dialog-week-chooser.c',
  'src/task-list-date-dialog.c',
  'src/task-list-notes-dialog.c',
  'src/task-list-priority-dialog.c',
  'src/task-list-sort-dialog.c',
  'src/task-list-tags-dialog-tag.c',
  'src/task-list-tags-dialog.c',
  'src/task-list.c',
  'src/task.c',
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

# UI files
blueprints = custom_target(
  'blueprints',
  input: files(
    'src/sidebar-all-row.blp',
    'src/sidebar-delete-list-dialog.blp',
    'src/sidebar-new-list-dialog.blp',
    'src/sidebar-rename-list-dialog.blp',
    'src/sidebar-task-list-row.blp',
    'src/sidebar.blp',
    'src/task-list-attachments-dialog-attachment.blp',
    'src/task-list-attachments-dialog.blp',
    'src/task-list-color-dialog.blp',
    'src/task-list-date-dialog-date-chooser.blp',
    'src/task-list-date-dialog-month-chooser.blp',
    'src/task-list-date-dialog-rrule-row.blp',
    'src/task-list-date-dialog-time-chooser.blp',
    'src/task-list-date-dialog-week-chooser.blp',
    'src/task-list-date-dialog.blp',
    'src/task-list-notes-dialog.blp',
    'src/task-list-priority-dialog.blp',
    'src/task-list-sort-dialog.blp',
    'src/task-list-tags-dialog-tag.blp',
    'src/task-list-tags-dialog.blp',
    'src/task-list.blp',
    'src/window.blp',
  ),
  output: '.',
  command: [
    find_program('blueprint-compiler'),
    'batch-compile',
    '@OUTPUT@',
    '@CURRENT_SOURCE_DIR@',
    '@INPUT@',
  ],
)

# Resources
gnome = import('gnome')
resources = gnome.compile_resources(
  meson.project_name(),
  'data' / 'errands.gresource.xml',
  c_name: meson.project_name(),
  dependencies: blueprints,
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
