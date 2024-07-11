#!/usr/bin/bash

build-aux/req2flatpak.py --requirements-file requirements.txt --target-platforms 311-x86_64 311-aarch64 > manifest.json
