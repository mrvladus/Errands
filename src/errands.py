#!@PYTHON@

# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import os
import sys
import signal
import locale
import gettext
import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")


APP_ID = "@APP_ID@"
VERSION = "@VERSION@"
PREFIX = "@PREFIX@"
PROFILE = "@PROFILE@"
pkgdatadir = "@pkgdatadir@"
localedir = "@localedir@"

sys.path.insert(1, pkgdatadir)
signal.signal(signal.SIGINT, signal.SIG_DFL)
gettext.install("errands", localedir)
locale.bindtextdomain("errands", localedir)
locale.textdomain("errands")

if __name__ == "__main__":
    from gi.repository import Gio

    resource = Gio.Resource.load(os.path.join(pkgdatadir, "errands.gresource"))
    resource._register()

    from errands.utils import Log

    Log.init()

    from errands import application

    app = application.Application()
    sys.exit(app.run(sys.argv))
