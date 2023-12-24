# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Gtk


class Box(Gtk.Box):
    """
    Box with multiple children provided in list.
    """

    def __init__(self, children: list[Gtk.Widget], **kwargs):
        super().__init__(**kwargs)
        for child in children:
            self.append(child)
