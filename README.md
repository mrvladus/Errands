
<div align="center">
  <img src="data/icons/io.github.mrvladus.List.svg" width="200" height="200">

  # Errands

  Manage your tasks
  
  <a href="https://circle.gnome.org/"><img src="https://circle.gnome.org/assets/button/badge.svg"></a>
  
  ![CI](https://github.com/mrvladus/Errands/actions/workflows/CI.yml/badge.svg)
  
<p align="center">
  <img src="screenshots/main.png" width="360" align="center">
  <img src="screenshots/secondary.png" width="360" align="center">
</p>

</div>

## Features
- Multiple task lists support
- Add, remove, edit tasks and sub-tasks
- Mark task and sub-tasks as completed
- Add accent color for each task
- Sync tasks with Nextcloud or other CalDAV providers
- Drag and Drop support

## Sync
Errands is using [python-caldav](https://github.com/python-caldav/caldav) library for syncing with Nextcloud Tasks and other CalDAV providers.

For now, only **Nextcloud** is supported and known to work well.
Other providers **may not work**. See [python-caldav  documentation](https://caldav.readthedocs.io/en/latest/#compatibility) for more info on compatibility.

## Install
### Flatpak
Errands is available as a Flatpak on Flathub:

<a href="https://flathub.org/apps/details/io.github.mrvladus.List"><img width='240' alt='Download on Flathub' src='https://dl.flathub.org/assets/badges/flathub-badge-en.png'/></a>

### Building using GNOME Builder
1. Install [GNOME Builder](https://flathub.org/apps/org.gnome.Builder).
2. Click "Clone Repository" with `https://github.com/mrvladus/Errands.git` as the URL.
3. Click on the build button at the top.

## Contribute

### Report a bug
- Make sure you are using latest version from [flathub](https://flathub.org/apps/details/io.github.mrvladus.List).
- See the log file at `~/.var/app/io.github.mrvladus.List/data/errands/log.txt` if it has any errors.
- If there is no errors in log file, then launch app from terminal with `flatpak run io.github.mrvladus.List` and copy the output.
- Create new issue.
- Add steps to reproduce the bug if needed.

### Translate
To translate **Errands** to your language see **[TRANSLATIONS.md](TRANSLATIONS.md)**

### Donate
If you like this app, you can support its development. See **[DONATIONS.md](DONATIONS.md)**

## Code of conduct

Errands follows the GNOME project [Code of Conduct](https://wiki.gnome.org/Foundation/CodeOfConduct).
