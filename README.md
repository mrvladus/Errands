<div align="center">
  <img src="data/icons/io.github.mrvladus.List.svg" width="200" height="200">

<h1>Errands</h1>

<p>Manage your tasks<p>

<a href="https://circle.gnome.org/">
  <img src="https://circle.gnome.org/assets/button/badge.svg" align="center">
</a>

<p align="center">
    <img src="data/screenshots/main.png" width="500" align="center">
    <img src="data/screenshots/properties.png" width="500" align="center">
</p>

</div>

## Install

Install official flatpak from Flathub:

<a href='https://flathub.org/apps/io.github.mrvladus.List'>
    <img width='240' alt='Get it on Flathub' src='https://flathub.org/api/badge?locale=en'/>
</a>

## Build

Use [Gnome Builder](https://flathub.org/en/apps/org.gnome.Builder) for building Flatpak.

## Develop

Setup distrobox container:

```sh
make distrobox-setup
```

Build and run:

```sh
make DEVEL=TRUE run
```

Cleanup:

```sh
make clean
```

Look into [Makefile](./Makefile) for more options.
