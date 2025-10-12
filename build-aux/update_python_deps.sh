#!/usr/bin/bash

./req2flatpak.py --requirements-file requirements.txt --target-platforms '313-x86_64' '313-aarch64' > python3-caldav.json
