#!@PYTHON@

# MIT License

# Copyright (c) 2023 Vlad Krupinski <mrvladus@yandex.ru>

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

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
pkgdatadir = "@pkgdatadir@"
localedir = "@localedir@"

sys.path.insert(1, pkgdatadir)
signal.signal(signal.SIGINT, signal.SIG_DFL)
gettext.install("list", localedir)
locale.bindtextdomain("list", localedir)
locale.textdomain("list")

if __name__ == "__main__":
    from gi.repository import Gio

    resource = Gio.Resource.load(os.path.join(pkgdatadir, "errands.gresource"))
    resource._register()

    from list.utils import Log

    Log.init()

    from list import application

    app = application.Application()
    sys.exit(app.run(sys.argv))
