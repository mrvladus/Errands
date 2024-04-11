# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from typing import Callable

from gi.repository import Gtk  # type:ignore

from errands.lib.utils import get_children


class ErrandsListBox(Gtk.ListBox):
    def __init__(
        self,
        children: list[Gtk.Widget],
        on_row_activated: Callable = None,
        on_row_selected: Callable = None,
        **kwargs,
    ) -> None:
        super().__init__(**kwargs)

        for child in children:
            self.append(child)

        if on_row_selected:
            self.connect("row-selected", on_row_selected)

        if on_row_activated:
            self.connect("row-activated", on_row_activated)

    @property
    def children(self) -> list[Gtk.Widget]:
        return get_children(self)

    def for_each(self, func: Callable) -> None:
        """Call func for each child. Child passed as first argument"""

        for child in self.children:
            func(child)
