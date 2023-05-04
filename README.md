# List

Focus on your tasks.
Todo application for those who prefer simplicity. Add and edit tasks and sub-tasks, change accent colors, remove completed.

## Screenshots
<a href="./screenshots/light.png"><img src="./screenshots/light.png" height="400"></a>
<a href="./screenshots/light.png"><img src="./screenshots/dark.png" height="400"></a>

## Installing
List is available as a Flatpak on Flathub:

<a href="https://flathub.org/apps/details/io.github.mrvladus.List"><img width='240' alt='Download on Flathub' src='https://dl.flathub.org/assets/badges/flathub-badge-en.png'/></a>

## Building from source
1. Clone repo
```
git clone https://github.com/mrvladus/List && cd List/
```
2. Install dependencies
```
flatpak install org.gnome.{Platform,Sdk}//44 org.flatpak.Builder org.freedesktop.appstream-glib -y
```
3. Run
```
make run
```
