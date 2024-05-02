#!/usr/bin/bash

DEPS="caldav lxml pip"
IGNORE_INSTALLED="lxml,pip"
RUNTIME="org.gnome.Sdk//46"

build-aux/flatpak-pip-generator.py --ignore-installed=$IGNORE_INSTALLED --output build-aux/python3-modules $DEPS