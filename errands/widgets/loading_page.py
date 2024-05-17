# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from gi.repository import Adw, Gtk  # type:ignore

from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox
from errands.widgets.shared.components.toolbar_view import ErrandsToolbarView


class ErrandsLoadingPage(Adw.Bin):
    def __init__(self):
        super().__init__()
        State.loading_page = self
        self.__build_ui()

    def __build_ui(self):
        self.set_child(
            ErrandsToolbarView(
                top_bars=[Adw.HeaderBar(show_title=False)],
                content=ErrandsBox(
                    orientation=Gtk.Orientation.VERTICAL,
                    children=[
                        Adw.StatusPage(
                            title=_("Loading Tasks..."),
                            icon_name="errands-progressbar-symbolic",
                            vexpand=True,
                            css_classes=["compact"],
                        )
                    ],
                ),
            )
        )
