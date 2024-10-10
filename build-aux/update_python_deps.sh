#!/usr/bin/bash

./req2flatpak.py --requirements-file requirements.txt --target-platforms 312-x86_64 312-aarch64 > manifest.json
