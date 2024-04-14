# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from typing import Callable

from gi.repository import Gtk  # type:ignore


class ErrandsButton(Gtk.Button):
    def __init__(self, on_click: Callable = None, **kwargs) -> None:
        super().__init__(**kwargs)
        if on_click:
            self.connect("clicked", on_click)


class ErrandsCheckButton(Gtk.CheckButton):
    def __init__(self, on_toggle: Callable = None, **kwargs) -> None:
        super().__init__(**kwargs)
        if on_toggle:
            self.connect("toggled", on_toggle)


class ErrandsToggleButton(Gtk.ToggleButton):
    def __init__(self, on_toggle: Callable = None, **kwargs) -> None:
        super().__init__(**kwargs)
        if on_toggle:
            self.connect("toggled", on_toggle)
