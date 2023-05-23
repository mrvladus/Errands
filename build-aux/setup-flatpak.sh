#!/usr/bin/bash
echo "INSTALLING DEPENDENCIES..."
flatpak install org.gnome.{Platform,Sdk}//44 org.flatpak.Builder org.freedesktop.appstream-glib -y
echo "DONE!"
