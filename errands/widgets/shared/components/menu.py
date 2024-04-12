# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from typing import Self
from gi.repository import Gio  # type:ignore


class ErrandsMenu(Gio.Menu):
    def __init__(self, items: list[Self]) -> None:
        super().__init__()

    def add_section(self) -> None:
        new_section = ErrandsMenu()
        self.append_section(new_section)
