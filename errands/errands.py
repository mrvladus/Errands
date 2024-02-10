#!@PYTHON@

# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import os
import sys
import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
gi.require_version("Secret", "1")
gi.require_version("GtkSource", "5")


APP_ID = "@APP_ID@"
VERSION = "@VERSION@"
PREFIX = "@PREFIX@"
PROFILE = "@PROFILE@"
pkgdatadir = "@pkgdatadir@"
localedir = "@localedir@"


def setup_gettext():
    import signal
    import locale
    import gettext

    sys.path.insert(1, pkgdatadir)
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    gettext.install("errands", localedir)
    locale.bindtextdomain("errands", localedir)
    locale.textdomain("errands")


def register_resources():
    from gi.repository import Gio  # type:ignore

    resource = Gio.Resource.load(os.path.join(pkgdatadir, "errands.gresource"))
    resource._register()


def init_logging():
    from errands.lib.logging import Log

    Log.init()


def init_user_data():
    from errands.lib.data import UserData

    UserData.init()


def main() -> None:
    setup_gettext()
    register_resources()
    init_logging()
    init_user_data()
    from errands.application import ErrandsApplication

    sys.exit(ErrandsApplication().run(sys.argv))


if __name__ == "__main__":
    main()
