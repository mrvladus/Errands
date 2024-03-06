# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import os
from gi.repository import Gtk  # type:ignore


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TitledSeparator(Gtk.Box):
    __gtype_name__ = "TitledSeparator"

    label: Gtk.Label = Gtk.Template.Child()

    def __init__(self, title: str):
        super().__init__()
        self.label.set_label(title)
