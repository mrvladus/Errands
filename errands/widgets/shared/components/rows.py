# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from typing import Callable

from gi.repository import Adw  # type:ignore


class ErrandsEntryRow(Adw.EntryRow):
    def __init__(self, on_entry_activated: Callable, **kwargs) -> None:
        super().__init__(**kwargs)
        self.connect("entry-activated", on_entry_activated)
