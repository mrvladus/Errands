# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from gi.repository import Gtk  # type:ignore

from errands.lib.data import UserData
from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox


class TagsSidebarRow(Gtk.ListBoxRow):
    def __init__(self) -> None:
        super().__init__()
        self.name = "errands_tags_page"
        State.tags_sidebar_row = self
        self.__build_ui()
        self.update_ui()

    def __build_ui(self) -> None:
        self.props.height_request = 45
        self.add_css_class("sidebar-item")
        self.connect("activate", self._on_row_activated)

        # Icon
        self.icon = Gtk.Image(icon_name="errands-tag-symbolic")

        # Title
        self.label: Gtk.Label = Gtk.Label(
            hexpand=True, halign=Gtk.Align.START, label=_("Tags")
        )

        # Counter
        self.size_counter = Gtk.Button(
            css_classes=["dim-label", "caption", "flat", "circular"],
            halign=Gtk.Align.CENTER,
            valign=Gtk.Align.CENTER,
            can_target=False,
        )

        self.set_child(
            ErrandsBox(
                spacing=12,
                margin_start=6,
                children=[self.icon, self.label, self.size_counter],
            )
        )

    def update_ui(self) -> None:
        size: int = len(UserData.tags)
        self.size_counter.set_label(str(size) if size > 0 else "")

    def _on_row_activated(self, *args) -> None:
        Log.debug("Sidebar: Open Tags")

        State.view_stack.set_visible_child_name(self.name)
        State.split_view.set_show_content(True)
        GSettings.set("last-open-list", "s", self.name)
        State.tags_page.update_ui()
