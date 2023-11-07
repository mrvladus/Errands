# Copyright 2023 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT

from errands.widgets.trash import Trash
from gi.repository import Adw, Gtk, GObject


class Sidebar(Adw.Bin):
    def __init__(self):
        super().__init__()
        self.build_ui()

    def build_ui(self):
        self.props.width_request = 360
        self.props.height_request = 200
        trash = Adw.ViewStackPage(
            child=Trash(), name="trash", icon_name="user-trash-symbolic"
        )
        details = Adw.ViewStackPage(
            child=Details(), name="details", icon_name="help-about-symbolic"
        )
        # Stack
        self.stack = Adw.ViewStack()
        self.stack.add(trash)
        self.stack.add(details)
        # Toolbar View
        toolbar_view = Adw.ToolbarView(content=self.stack)
        toolbar_view.add_bottom_bar(Adw.ViewSwitcherBar(stack=self.stack, reveal=True))
        self.set_child(toolbar_view)
