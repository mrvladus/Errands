# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from typing import Callable

from gi.repository import Gtk  # type:ignore


class ErrandsButton(Gtk.Button):
    def __init__(self, on_click: Callable, **kwargs) -> None:
        super().__init__(**kwargs)
        self.connect("clicked", on_click)
