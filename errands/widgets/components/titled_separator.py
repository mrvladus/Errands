# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

import os
from gi.repository import Gtk, GObject  # type:ignore


@Gtk.Template(filename=os.path.abspath(__file__).replace(".py", ".ui"))
class TitledSeparator(Gtk.Box):
    __gtype_name__ = "TitledSeparator"

    # PROPERTIES
    title = GObject.Property(type=str, default="")

    label: Gtk.Label = Gtk.Template.Child()

    def __init__(
        self, title: str = "", margins: tuple[int, int, int, int] = (0, 0, 0, 0)
    ):
        super().__init__()
        self.label.set_label(title)
        self.set_margin_start(margins[0])
        self.set_margin_end(margins[1])
        self.set_margin_top(margins[2])
        self.set_margin_bottom(margins[3])
