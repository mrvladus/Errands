# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from typing import Callable
from gi.repository import Gtk  # type:ignore

from errands.lib.utils import get_children


class ErrandsBox[T](Gtk.Box):
    def __init__(self, children: list[T], **kwargs) -> None:
        super().__init__(**kwargs)
        for child in children:
            self.append(child)

    @property
    def children(self) -> list[T]:
        return get_children(self)

    def for_each(self, func: Callable) -> None:
        """Call func for each child. Child passed as first argument"""

        for child in self.children:
            func(child)
