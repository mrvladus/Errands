# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk, GObject


class Sidebar(Adw.Bin):
    def __init__(self):
        super().__init__()
