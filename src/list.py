#!@PYTHON@
import os
import sys
import signal
import locale
import gettext

VERSION = "@VERSION@"
pkgdatadir = "@pkgdatadir@"
localedir = "@localedir@"

sys.path.insert(1, pkgdatadir)
signal.signal(signal.SIGINT, signal.SIG_DFL)
locale.bindtextdomain("list", localedir)
locale.textdomain("list")
gettext.install("list", localedir)

if __name__ == "__main__":

    from gi.repository import Gio

    resource = Gio.Resource.load(
        os.path.join(pkgdatadir, "io.github.mrvladus.List.gresource")
    )
    resource._register()

    from list import main

    app = main.Application()
    sys.exit(app.run(sys.argv))
