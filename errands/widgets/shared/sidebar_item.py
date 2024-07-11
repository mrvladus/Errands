# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Gio, Gtk  # type:ignore


class ErrandsSidebarItem(Gtk.Box):
    def __init__(
        self,
        title: str | None = None,
        icon_name: str | None = None,
        menu_model: Gio.Menu | None = None,
    ):
        super().__init__()
        self.title: str = title
        self.icon_name: str = icon_name
        self.menu_model: Gio.Menu = menu_model
        self.__build_ui()

    def __build_ui(self) -> None:
        pass
