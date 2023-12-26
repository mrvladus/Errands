# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from typing import Callable
from gi.repository import Gtk, Adw


class Button(Gtk.Button):
    """
    Button which takes as arguments: label, icon and callback for "clicked" signal.
    """

    def __init__(
        self,
        label: str = None,
        icon_name: str = None,
        on_click: Callable = None,
        **kwargs
    ):
        super().__init__(**kwargs)
        content = Adw.ButtonContent()
        if icon_name:
            content.set_icon_name(icon_name)
        if label:
            content.set_label(label)
        if on_click:
            self.connect("clicked", on_click)
        self.set_child(content)
