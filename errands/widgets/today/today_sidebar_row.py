# Copyright 2024 Vlad Krupinskii <mrvladus@yandex.ru>
# SPDX-License-Identifier: MIT


from gi.repository import Gtk  # type:ignore

from errands.lib.gsettings import GSettings
from errands.lib.logging import Log
from errands.state import State
from errands.widgets.shared.components.boxes import ErrandsBox


class TodaySidebarRow(Gtk.ListBoxRow):
    def __init__(self) -> None:
        super().__init__()
        self.name = "errands_today_page"
        State.today_sidebar_row = self
        self.__build_ui()

    def __build_ui(self):
        self.props.height_request = 50
        self.add_css_class("sidebar-item")
        self.connect("activate", self._on_row_activated)

        # Icon
        self.icon = Gtk.Image(icon_name="errands-calendar-symbolic")

        # Title
        self.label: Gtk.Label = Gtk.Label(
            hexpand=True, halign=Gtk.Align.START, label=_("Today")
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

    def update_ui(self):
        State.today_page.update_ui()

    def _on_row_activated(self, *args) -> None:
        Log.debug("Sidebar: Open Today")
        State.view_stack.set_visible_child_name(self.name)
        State.split_view.set_show_content(True)
        GSettings.set("last-open-list", "s", self.name)
        State.today_page.update_ui()
