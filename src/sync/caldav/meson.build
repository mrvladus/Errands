libcaldav = static_library(
  'caldav',
  sources: [
    'caldav-calendar.c',
    'caldav-client.c',
    'caldav-event.c',
    'caldav-list.c',
    'caldav-requests.c',
  ],
  dependencies: [dependency('libcurl'), dependency('libical')],
)

install_headers(
  'caldav.h',
  install_dir: get_option('includedir') / 'caldav',
)
