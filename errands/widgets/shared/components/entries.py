# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from typing import Callable

from gi.repository import Adw, Gtk  # type:ignore


class ErrandsEntryRow(Adw.EntryRow):
    def __init__(self, on_entry_activated: Callable, **kwargs) -> None:
        super().__init__(**kwargs)
        self.connect("entry-activated", on_entry_activated)


class ErrandsEntry(Gtk.Entry):
    def __init__(self, on_activate: Callable, **kwargs) -> None:
        super().__init__(**kwargs)
        self.connect("activate", on_activate)
